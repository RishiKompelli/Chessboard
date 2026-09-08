import time
import msvcrt
import chess
import chess.engine
import serial


# ---------------- USER SETTINGS ----------------

SERIAL_PORT = "COM3"
BAUD_RATE = 9600

STOCKFISH_PATH = r"C:\Users\rishi_7b7n0gh\Downloads\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2.exe"

ENGINE_THINK_TIME_SECONDS = 0.5

# Captures are manual again.
# Python will ask you to remove the captured piece by hand.
CAPTURE_STYLE = "manual_remove"
# CAPTURE_STYLE = "parking"


# ---------------- SERIAL HELPERS ----------------

def open_arduino():
    print(f"Opening Arduino on {SERIAL_PORT}...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)

    time.sleep(2.5)
    read_available_lines(ser, 1.0)

    print()
    print("Put the carriage/electromagnet exactly at the center of a1.")
    input("Press Enter when it is at a1...")

    send_arduino_command(ser, "q", expected_ok="OK POSITION_SET_A1", timeout=10)
    send_arduino_command(ser, "f", expected_ok=None, timeout=2)

    return ser


def read_available_lines(ser, duration=0.5):
    start = time.time()

    while time.time() - start < duration:
        while ser.in_waiting > 0:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                print("Arduino:", line)

        time.sleep(0.02)


def send_raw_char(ser, ch):
    ser.write(ch.encode())
    ser.flush()


def abort_arduino_input(ser):
    ser.reset_input_buffer()

    ser.write(b"!\n")
    ser.flush()

    time.sleep(0.25)

    while ser.in_waiting > 0:
        line = ser.readline().decode(errors="ignore").strip()
        if line:
            print("Arduino:", line)


def send_arduino_command(ser, command, expected_ok=None, timeout=600):
    command = command.strip().lower().replace(" ", "")

    if command == "":
        return True

    print()
    print(f"> Arduino command: {command}")

    abort_arduino_input(ser)

    ser.reset_input_buffer()

    for ch in command:
        ser.write(ch.encode())
        ser.flush()
        time.sleep(0.02)

    ser.write(b"\n")
    ser.flush()

    if expected_ok is None:
        read_available_lines(ser, 1.0)
        return True

    expected_ok = expected_ok.upper()
    start = time.time()

    while time.time() - start < timeout:
        while ser.in_waiting > 0:
            line = ser.readline().decode(errors="ignore").strip()

            if not line:
                continue

            print("Arduino:", line)

            upper_line = line.upper()

            if upper_line.startswith("ERR"):
                print("Arduino returned an error.")
                return False

            if expected_ok in upper_line:
                print("Arduino finished command.")
                return True

        time.sleep(0.02)

    print()
    print("Timed out waiting for Arduino.")
    print(f"Expected: {expected_ok}")
    return False


# ---------------- LIVE JOG MODE ----------------

def jog_mode(ser):
    print()
    print("LIVE JOG MODE")
    print("No Enter needed.")
    print()
    print("Controls:")
    print("  w / up arrow     = move up")
    print("  s / down arrow   = move down")
    print("  a / left arrow   = move left/right")
    print("  d / right arrow  = move left/right")
    print("  space or x       = stop")
    print("  f                = force magnet off")
    print("  v                = toggle magnet")
    print("  +                = bigger jog step")
    print("  -                = smaller jog step")
    print("  p                = print position")
    print("  q                = set current position as a1")
    print("  c                = start 4-corner calibration")
    print("  k                = save calibration point")
    print("  g                = print grid")
    print("  u                = status")
    print("  b                = print board")
    print("  i                = reset board")
    print("  h                = Arduino help")
    print("  Esc              = exit jog mode")
    print()

    last_sent = None

    while True:
        while ser.in_waiting > 0:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                print("Arduino:", line)

        if msvcrt.kbhit():
            key = msvcrt.getwch()

            if key in ("\x00", "\xe0"):
                arrow = msvcrt.getwch()

                if arrow == "H":
                    key = "w"
                elif arrow == "P":
                    key = "s"
                elif arrow == "K":
                    key = "a"
                elif arrow == "M":
                    key = "d"
                else:
                    continue

            if key == "\x1b":
                send_raw_char(ser, "x")
                time.sleep(0.05)
                send_raw_char(ser, "f")
                print()
                print("Exiting jog mode.")
                return

            if key == " ":
                key = "x"

            key = key.lower()

            allowed_keys = [
                "w", "a", "s", "d",
                "x",
                "+", "-",
                "p", "q", "c", "k", "g", "u", "b", "i", "h",
                "o", "f", "v",
                "1", "2", "3", "4",
                "!"
            ]

            if key in allowed_keys:
                if key in ["w", "a", "s", "d"] and key == last_sent:
                    continue

                send_raw_char(ser, key)
                last_sent = key

                if key == "x":
                    print("STOP")
                    last_sent = None
                elif key == "f":
                    print("FORCE MAGNET OFF")
                    last_sent = None
                elif key == "!":
                    print("ABORT INPUT")
                    last_sent = None
                elif key in ["w", "a", "s", "d"]:
                    print("Moving:", key)
                else:
                    print("Sent:", key)
                    last_sent = None

            else:
                print("Ignored key:", repr(key))

        time.sleep(0.01)


# ---------------- ARDUINO COMMAND PARSING ----------------

def normalize_manual_command(text):
    text = text.strip().lower()

    if text == "":
        return ""

    parts = text.split()

    if len(parts) == 1:
        return parts[0]

    first = parts[0]
    rest = "".join(parts[1:])

    if first in ["r", "y", "l", "n", "e"]:
        return first + rest

    return text.replace(" ", "")


def expected_ok_for_manual_command(command):
    command = command.strip().lower().replace(" ", "")

    if command == "q":
        return "OK POSITION_SET_A1"

    if command == "f" or command == "o":
        return None

    if command == "!":
        return "OK ABORT_INPUT"

    if command == "u":
        return "OK STATUS"

    if command.startswith("r") and len(command) == 5:
        return "OK MOVE_COMMAND"

    if command.startswith("y") and len(command) == 6:
        return "OK CAPTURE_COMMAND"

    if command.startswith("l") and len(command) == 3:
        return "OK CASTLE_COMMAND"

    if command.startswith("n") and len(command) == 6:
        return "OK PROMOTION_COMMAND"

    if command.startswith("e") and len(command) == 8:
        return "OK EN_PASSANT_COMMAND"

    return None


# ---------------- CHESS HELPERS ----------------

def color_char(chess_color):
    return "w" if chess_color == chess.WHITE else "b"


def promotion_char(piece_type):
    if piece_type == chess.QUEEN:
        return "q"
    if piece_type == chess.ROOK:
        return "r"
    if piece_type == chess.BISHOP:
        return "b"
    if piece_type == chess.KNIGHT:
        return "n"

    return "q"


def build_arduino_command(board, move):
    from_square = chess.square_name(move.from_square)
    to_square = chess.square_name(move.to_square)

    moving_color = board.turn
    moving_color_letter = color_char(moving_color)

    if board.is_castling(move):
        from_file = chess.square_file(move.from_square)
        to_file = chess.square_file(move.to_square)
        side = "k" if to_file > from_file else "q"

        return {
            "command": "l" + moving_color_letter + side,
            "expected_ok": "OK CASTLE_COMMAND",
            "description": "castling",
        }

    if board.is_en_passant(move):
        captured_square = chess.square(
            chess.square_file(move.to_square),
            chess.square_rank(move.from_square)
        )

        captured_square_name = chess.square_name(captured_square)
        captured_color_letter = color_char(not moving_color)

        return {
            "command": "e" + from_square + to_square + captured_square_name + captured_color_letter,
            "expected_ok": "OK EN_PASSANT_COMMAND",
            "description": "en passant manual remove",
            "manual_remove_square": captured_square_name,
        }

    if move.promotion is not None:
        promo = promotion_char(move.promotion)

        move_info = {
            "command": "n" + from_square + to_square + promo,
            "expected_ok": "OK PROMOTION_COMMAND",
            "description": "promotion",
        }

        if board.is_capture(move):
            move_info["manual_remove_square"] = to_square

        return move_info

    if board.is_capture(move):
        return {
            "command": "r" + from_square + to_square,
            "expected_ok": "OK MOVE_COMMAND",
            "description": "capture manual remove",
            "manual_remove_square": to_square,
        }

    return {
        "command": "r" + from_square + to_square,
        "expected_ok": "OK MOVE_COMMAND",
        "description": "normal move",
    }


def execute_move_on_arduino(ser, board, move, label):
    move_info = build_arduino_command(board, move)

    if "manual_remove_square" in move_info:
        square = move_info["manual_remove_square"]
        print()
        print(f"{label} move captures a piece on {square}.")
        input(f"Remove the captured piece from {square}, then press Enter...")

    print(f"{label} move type: {move_info['description']}")
    print(f"{label} move command: {move_info['command']}")

    move_ok = send_arduino_command(
        ser,
        move_info["command"],
        expected_ok=move_info["expected_ok"],
        timeout=600
    )

    return move_ok


def parse_move(board, text):
    text = text.strip()

    try:
        move = chess.Move.from_uci(text.lower())
        if move in board.legal_moves:
            return move
    except ValueError:
        pass

    try:
        move = board.parse_san(text)
        if move in board.legal_moves:
            return move
    except ValueError:
        pass

    return None


def print_board(board):
    print()
    print(board)
    print()
    print("FEN:", board.fen())
    print()


# ---------------- FULL GAME MODE ----------------

def play_game(ser, engine, human_color=chess.WHITE):
    board = chess.Board()

    print()
    print("Starting full game.")
    print("Important: do NOT move pieces by hand unless Python tells you to remove a captured piece.")
    print("Type your move, then let the Arduino move your piece.")
    print()

    input("Set all real pieces to the starting position, then press Enter...")

    print()
    print("Checking Arduino status before game starts...")
    send_arduino_command(ser, "u", expected_ok="OK STATUS", timeout=10)

    print()
    print("If Arduino status said Board calibrated: no, stop and recalibrate before playing.")
    input("Press Enter to continue if Board calibrated was yes...")

    send_arduino_command(ser, "i", expected_ok=None, timeout=5)

    print()
    if human_color == chess.WHITE:
        print("You are white. You move first.")
    else:
        print("You are black. Stockfish moves first.")

    while not board.is_game_over():
        print_board(board)

        if board.turn == human_color:
            while True:
                user_text = input("Your move: ").strip()

                if user_text.lower() in ["quit", "exit"]:
                    print("Leaving game mode.")
                    send_arduino_command(ser, "f", expected_ok=None, timeout=2)
                    return

                if user_text.lower() == "resign":
                    print("You resigned.")
                    send_arduino_command(ser, "f", expected_ok=None, timeout=2)
                    return

                if user_text.lower() == "board":
                    print_board(board)
                    continue

                if user_text.lower() == "off":
                    send_arduino_command(ser, "f", expected_ok=None, timeout=2)
                    continue

                move = parse_move(board, user_text)

                if move is None:
                    print("Invalid or illegal move. Try e2e4, g1f3, O-O, etc.")
                    continue

                break

            print(f"You chose: {move.uci()}")

            success = execute_move_on_arduino(ser, board, move, "Your")

            if not success:
                print("Arduino failed to perform your move. Game paused.")
                send_arduino_command(ser, "f", expected_ok=None, timeout=2)
                return

            board.push(move)

        else:
            print("Stockfish is thinking...")

            result = engine.play(
                board,
                chess.engine.Limit(time=ENGINE_THINK_TIME_SECONDS)
            )

            move = result.move

            if move is None:
                print("Stockfish did not return a move.")
                send_arduino_command(ser, "f", expected_ok=None, timeout=2)
                return

            print(f"Stockfish move: {move.uci()}")

            success = execute_move_on_arduino(ser, board, move, "Stockfish")

            if not success:
                print("Arduino failed to perform Stockfish's move. Game paused.")
                send_arduino_command(ser, "f", expected_ok=None, timeout=2)
                return

            board.push(move)

    send_arduino_command(ser, "f", expected_ok=None, timeout=2)

    print_board(board)
    print("Game over.")
    print("Result:", board.result())
    print("Outcome:", board.outcome())


# ---------------- MAIN SERIAL MONITOR MODE ----------------

def print_menu():
    print()
    print("Commands:")
    print("  jog           live manual movement, no Enter needed")
    print("  game          start full game, you play white")
    print("  game black    start full game, you play black")
    print("  !             abort Arduino input mode")
    print("  f             force magnet off")
    print("  q             set current carriage position as a1")
    print("  u             print Arduino status")
    print("  b             print Arduino board state")
    print("  i             reset Arduino board state")
    print("  g             print grid")
    print("  c/k           calibration commands")
    print("  r e2e4        Arduino regular move")
    print("  y e4d5b       old automatic capture command, now disabled")
    print("  l wk          Arduino castle mode")
    print("  n e7e8q       Arduino promotion mode")
    print("  e e5d6d5b     en passant manual remove")
    print("  h             Arduino help")
    print("  menu          show this menu")
    print("  quit          exit")
    print()


def serial_monitor_loop(ser, engine):
    print_menu()

    while True:
        text = input("Chessboard> ").strip()

        if text == "":
            read_available_lines(ser, 0.5)
            continue

        lower = text.lower()

        if lower in ["quit", "exit"]:
            send_arduino_command(ser, "f", expected_ok=None, timeout=2)
            return

        if lower == "menu":
            print_menu()
            continue

        if lower == "jog":
            jog_mode(ser)
            continue

        if lower == "game":
            play_game(ser, engine, human_color=chess.WHITE)
            continue

        if lower == "game black":
            play_game(ser, engine, human_color=chess.BLACK)
            continue

        command = normalize_manual_command(text)
        expected_ok = expected_ok_for_manual_command(command)

        send_arduino_command(
            ser,
            command,
            expected_ok=expected_ok,
            timeout=600
        )


def main():
    print("Starting Chessboard Serial Monitor + Stockfish Controller")
    print()

    ser = None
    engine = None

    try:
        ser = open_arduino()

        print()
        print(f"Opening Stockfish: {STOCKFISH_PATH}")
        engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
        print("Stockfish ready.")

        serial_monitor_loop(ser, engine)

    except serial.SerialException as e:
        print("Serial error:")
        print(e)
        print()
        print("Make sure PlatformIO Serial Monitor is closed.")
        print("Only one program can use COM3 at a time.")

    except FileNotFoundError:
        print("Could not find Stockfish.")
        print("Check STOCKFISH_PATH at the top of this file.")

    finally:
        print()
        print("Closing program.")

        if ser is not None and ser.is_open:
            try:
                send_arduino_command(ser, "f", expected_ok=None, timeout=2)
            except Exception:
                pass

        if engine is not None:
            engine.quit()

        if ser is not None and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()