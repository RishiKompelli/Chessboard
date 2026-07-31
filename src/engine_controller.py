import time
import chess
import chess.engine
import serial

STOCKFISH_PATH = r"C:\Users\rishi_7b7n0gh\Downloads\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2.exe"

SERIAL_PORT = "COM3"
BAUD_RATE = 9600
ENGINE_THINK_TIME = 0.5


def wait_for_arduino_done(ser, timeout=60):
    start = time.time()

    while time.time() - start < timeout:
        line = ser.readline().decode(errors="ignore").strip()

        if line:
            print("Arduino:", line)

        if "move complete" in line.lower():
            return True

        if "failed" in line.lower() or "aborted" in line.lower():
            return False

    print("Timed out waiting for Arduino.")
    return False


def send_move_to_arduino(ser, move):
    move_text = move.uci()

    if len(move_text) != 4:
        print("Special move not supported yet:", move_text)
        return False

    command = "r" + move_text + "\n"

    print("Sending to Arduino:", command.strip())
    ser.write(command.encode())

    return wait_for_arduino_done(ser)


def main():
    board = chess.Board()

    print("Opening Arduino on", SERIAL_PORT)
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)

    print("Starting Stockfish")
    engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)

    try:
        while not board.is_game_over():
            print()
            print(board)
            print()
            print("FEN:", board.fen())

            human_text = input("Your move, like e2e4: ").strip().lower()

            try:
                human_move = chess.Move.from_uci(human_text)
            except ValueError:
                print("Invalid format. Use something like e2e4.")
                continue

            if human_move not in board.legal_moves:
                print("Illegal move.")
                continue

            board.push(human_move)

            print("Engine thinking...")
            result = engine.play(board, chess.engine.Limit(time=ENGINE_THINK_TIME))
            engine_move = result.move

            print("Engine move:", engine_move.uci())

            success = send_move_to_arduino(ser, engine_move)

            if success:
                board.push(engine_move)
            else:
                print("Arduino did not finish. Stopping.")
                break

        print("Game over:", board.result())

    finally:
        engine.quit()
        ser.close()


if __name__ == "__main__":
    main()