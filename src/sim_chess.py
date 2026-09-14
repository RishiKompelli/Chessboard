import pygame
import sys
import math
import threading
import queue
import chess
import chess.engine

# ============================================================
# SETTINGS
# ============================================================

STOCKFISH_PATH = r"C:\Users\rishi_7b7n0gh\Downloads\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2.exe"

WINDOW_W = 900
WINDOW_H = 860

FPS = 60

BOARD_SIZE = 640
BOARD_X = 130
BOARD_Y = 110
SQUARE_SIZE = BOARD_SIZE // 8

# Smaller = slower movement
MOVE_STEPS_PER_FRAME = 3

STOCKFISH_TIME_LIMIT = 0.4

# Visual motor spin amount
MOTOR_VISUAL_SCALE = 0.055

# Board transparency
LIGHT_ALPHA = 145
DARK_ALPHA = 130

# ============================================================
# COLORS
# ============================================================

BG = (30, 30, 46)
TEXT = (205, 214, 244)
MUTED = (166, 173, 200)
WARNING = (249, 226, 175)

LIGHT = (205, 214, 244, LIGHT_ALPHA)
DARK = (69, 71, 90, DARK_ALPHA)
GRID = (205, 214, 244, 130)

WHITE_PIECE = (245, 224, 220)
BLACK_PIECE = (17, 17, 27)

MAGNET = (243, 139, 168)
PATH = (137, 180, 250)
TARGET = (166, 227, 161)

BELT = (186, 194, 222)
RAIL = (147, 153, 178)
MOTOR = (203, 166, 247)
PULLEY = (250, 179, 135)
CARRIAGE = (69, 71, 90)

# ============================================================
# GAME STATE
# ============================================================

game_board = chess.Board()

engine = None
engine_queue = queue.Queue()
waiting_for_engine = False

command_text = ""
status_text = "Type a legal move like e2e4, then press Enter."
engine_status = "Stockfish not started yet."

magnet_x = 0
magnet_y = 0

moving = False
pending_board_move = None

path_points = []
corexy_frames = []
corexy_index = 0
trail = []

motor_a_steps = 0
motor_b_steps = 0
motor_a_angle = 0
motor_b_angle = 0

show_belts = False
show_path = True
show_board_overlay = True

# ============================================================
# COORDINATE HELPERS
# ============================================================

def square_to_pixel(square):
    file_index = chess.square_file(square)
    rank_index = chess.square_rank(square)

    x = BOARD_X + file_index * SQUARE_SIZE + SQUARE_SIZE / 2
    y = BOARD_Y + (7 - rank_index) * SQUARE_SIZE + SQUARE_SIZE / 2

    return x, y


def grid_to_pixel(file_coord, rank_coord):
    x = BOARD_X + file_coord * SQUARE_SIZE + SQUARE_SIZE / 2
    y = BOARD_Y + (7 - rank_coord) * SQUARE_SIZE + SQUARE_SIZE / 2
    return x, y


def screen_to_local(x, y):
    return x - BOARD_X, y - BOARD_Y


def local_to_screen(x, y):
    return BOARD_X + x, BOARD_Y + y


# ============================================================
# COREXY MATH
# ============================================================

def xy_to_ab_screen(x, y):
    """
    Convert screen X/Y into local board X/Y first,
    then convert that into CoreXY A/B motor positions.
    """
    lx, ly = screen_to_local(x, y)

    a = round(lx + ly)
    b = round(lx - ly)

    return a, b


def ab_to_xy_screen(a, b):
    """
    Reverse CoreXY:
      X = (A + B) / 2
      Y = (A - B) / 2
    """
    lx = (a + b) / 2
    ly = (a - b) / 2

    return local_to_screen(lx, ly)


def build_corexy_segment(x0, y0, x1, y1):
    a0, b0 = xy_to_ab_screen(x0, y0)
    a1, b1 = xy_to_ab_screen(x1, y1)

    da_total = a1 - a0
    db_total = b1 - b0

    steps = max(abs(da_total), abs(db_total))

    if steps == 0:
        return []

    frames = []

    prev_a = a0
    prev_b = b0

    for i in range(1, steps + 1):
        a = a0 + round(da_total * i / steps)
        b = b0 + round(db_total * i / steps)

        if a == prev_a and b == prev_b:
            continue

        x, y = ab_to_xy_screen(a, b)

        frames.append({
            "x": x,
            "y": y,
            "a": a,
            "b": b,
            "da": a - prev_a,
            "db": b - prev_b,
        })

        prev_a = a
        prev_b = b

    return frames


def build_corexy_frames(points):
    global magnet_x, magnet_y

    frames = []

    start_x = magnet_x
    start_y = magnet_y

    for target_x, target_y in points:
        segment = build_corexy_segment(start_x, start_y, target_x, target_y)
        frames.extend(segment)

        start_x = target_x
        start_y = target_y

    return frames


# ============================================================
# PATH PLANNING
# ============================================================

def is_diagonal_move(move):
    from_file = chess.square_file(move.from_square)
    from_rank = chess.square_rank(move.from_square)
    to_file = chess.square_file(move.to_square)
    to_rank = chess.square_rank(move.to_square)

    d_file = abs(to_file - from_file)
    d_rank = abs(to_rank - from_rank)

    return d_file == d_rank and d_file != 0


def should_use_direct_diagonal(move):
    piece = game_board.piece_at(move.from_square)

    if piece is None:
        return False

    # Knights should never go through the middle.
    if piece.piece_type == chess.KNIGHT:
        return False

    # These are allowed to use real diagonal movement.
    if piece.piece_type == chess.BISHOP and is_diagonal_move(move):
        return True

    if piece.piece_type == chess.QUEEN and is_diagonal_move(move):
        return True

    if piece.piece_type == chess.KING and is_diagonal_move(move):
        return True

    if piece.piece_type == chess.PAWN and is_diagonal_move(move):
        return True

    return False


def build_line_path(from_square, to_square):
    return [
        square_to_pixel(from_square),
        square_to_pixel(to_square),
    ]


def build_lane_path(from_square, to_square):
    from_file = chess.square_file(from_square)
    from_rank = chess.square_rank(from_square)
    to_file = chess.square_file(to_square)
    to_rank = chess.square_rank(to_square)

    d_file = to_file - from_file
    d_rank = to_rank - from_rank

    points = []

    # Source square
    points.append(grid_to_pixel(from_file, from_rank))

    if abs(d_rank) >= abs(d_file):
        # Mostly vertical movement, so use a file lane.
        lane_file = from_file + 0.5

        if d_file < 0 or (d_file == 0 and from_file == 7):
            lane_file = from_file - 0.5

        lane_rank = to_rank - 0.5

        if d_rank < 0 or (d_rank == 0 and to_rank == 0):
            lane_rank = to_rank + 0.5

        points.append(grid_to_pixel(lane_file, from_rank))
        points.append(grid_to_pixel(lane_file, lane_rank))
        points.append(grid_to_pixel(to_file, lane_rank))
        points.append(grid_to_pixel(to_file, to_rank))

    else:
        # Mostly horizontal movement, so use a rank lane.
        lane_rank = from_rank + 0.5

        if d_rank < 0 or (d_rank == 0 and from_rank == 7):
            lane_rank = from_rank - 0.5

        lane_file = to_file - 0.5

        if d_file < 0 or (d_file == 0 and to_file == 0):
            lane_file = to_file + 0.5

        points.append(grid_to_pixel(from_file, lane_rank))
        points.append(grid_to_pixel(lane_file, lane_rank))
        points.append(grid_to_pixel(lane_file, to_rank))
        points.append(grid_to_pixel(to_file, to_rank))

    return points


def build_path_for_move(move):
    if should_use_direct_diagonal(move):
        return build_line_path(move.from_square, move.to_square)

    return build_lane_path(move.from_square, move.to_square)


def get_path_name(move):
    if should_use_direct_diagonal(move):
        return "direct diagonal"

    return "safe lane"


# ============================================================
# STOCKFISH
# ============================================================

def start_engine():
    global engine, engine_status

    try:
        engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
        engine_status = "Stockfish ready."
    except Exception as e:
        engine = None
        engine_status = f"Stockfish failed: {e}"


def ask_stockfish_async():
    global waiting_for_engine, engine_status

    if engine is None:
        waiting_for_engine = False
        engine_status = "Stockfish is not running. Check STOCKFISH_PATH."
        return

    waiting_for_engine = True
    engine_status = "Stockfish thinking..."

    board_copy = game_board.copy()

    thread = threading.Thread(target=stockfish_worker, args=(board_copy,), daemon=True)
    thread.start()


def stockfish_worker(board_copy):
    try:
        result = engine.play(board_copy, chess.engine.Limit(time=STOCKFISH_TIME_LIMIT))
        engine_queue.put(result.move)
    except Exception as e:
        engine_queue.put(e)


def check_engine_result():
    global waiting_for_engine, engine_status

    if not waiting_for_engine:
        return

    try:
        result = engine_queue.get_nowait()
    except queue.Empty:
        return

    waiting_for_engine = False

    if isinstance(result, Exception):
        engine_status = f"Stockfish error: {result}"
        return

    if result is None:
        engine_status = "Stockfish returned no move."
        return

    engine_status = f"Stockfish chose {result.uci()}"
    start_animation_for_move(result, "Stockfish")


def maybe_start_stockfish_turn():
    if moving:
        return

    if pending_board_move is not None:
        return

    if waiting_for_engine:
        return

    if game_board.is_game_over():
        return

    if game_board.turn == chess.BLACK:
        ask_stockfish_async()


# ============================================================
# MOVE HANDLING
# ============================================================

def parse_user_move(text):
    text = text.strip().lower().replace(" ", "")

    if text.startswith("r") and len(text) >= 5:
        text = text[1:]

    if len(text) not in [4, 5]:
        return None

    try:
        move = chess.Move.from_uci(text)
    except ValueError:
        return None

    if move in game_board.legal_moves:
        return move

    # Auto queen promotion if user typed e7e8 instead of e7e8q.
    from_piece = game_board.piece_at(move.from_square)

    if from_piece is not None and from_piece.piece_type == chess.PAWN:
        to_rank = chess.square_rank(move.to_square)

        if to_rank == 0 or to_rank == 7:
            promoted = chess.Move(move.from_square, move.to_square, promotion=chess.QUEEN)

            if promoted in game_board.legal_moves:
                return promoted

    return None


def start_animation_for_move(move, label):
    global moving, pending_board_move, status_text
    global path_points, corexy_frames, corexy_index, trail

    path_points = build_path_for_move(move)
    corexy_frames = build_corexy_frames(path_points)

    corexy_index = 0
    trail = []
    moving = True
    pending_board_move = move

    path_name = get_path_name(move)

    status_text = f"{label}: {move.uci()} | path={path_name} | transparent board view"


def finish_current_move():
    global pending_board_move, status_text

    if pending_board_move is None:
        return

    san = game_board.san(pending_board_move)
    game_board.push(pending_board_move)

    if game_board.is_game_over():
        status_text = f"Move finished: {san} | Game over: {game_board.result()}"
    else:
        status_text = f"Move finished: {san}"

    pending_board_move = None


def handle_user_move(text):
    global status_text

    if game_board.is_game_over():
        status_text = f"Game over: {game_board.result()}"
        return

    if game_board.turn != chess.WHITE:
        status_text = "Wait for Stockfish first."
        return

    move = parse_user_move(text)

    if move is None:
        status_text = "Illegal move. Try something like e2e4."
        return

    start_animation_for_move(move, "Your move")


def reset_game():
    global game_board, moving, pending_board_move, waiting_for_engine
    global command_text, status_text, engine_status
    global magnet_x, magnet_y, path_points, corexy_frames, corexy_index, trail
    global motor_a_steps, motor_b_steps, motor_a_angle, motor_b_angle

    game_board = chess.Board()

    magnet_x, magnet_y = square_to_pixel(chess.A1)

    moving = False
    pending_board_move = None
    waiting_for_engine = False

    command_text = ""
    status_text = "Game reset. You are white. Type e2e4."
    engine_status = "Stockfish ready." if engine is not None else "Stockfish not running."

    path_points = []
    corexy_frames = []
    corexy_index = 0
    trail = []

    motor_a_steps, motor_b_steps = xy_to_ab_screen(magnet_x, magnet_y)
    motor_a_angle = motor_a_steps * MOTOR_VISUAL_SCALE
    motor_b_angle = motor_b_steps * MOTOR_VISUAL_SCALE


# ============================================================
# ANIMATION
# ============================================================

def update_motion():
    global moving, magnet_x, magnet_y, corexy_index
    global motor_a_steps, motor_b_steps, motor_a_angle, motor_b_angle

    if not moving:
        return

    if corexy_index >= len(corexy_frames):
        moving = False
        finish_current_move()
        return

    for _ in range(MOVE_STEPS_PER_FRAME):
        if corexy_index >= len(corexy_frames):
            moving = False
            finish_current_move()
            return

        frame = corexy_frames[corexy_index]

        magnet_x = frame["x"]
        magnet_y = frame["y"]

        motor_a_steps = frame["a"]
        motor_b_steps = frame["b"]

        motor_a_angle += frame["da"] * MOTOR_VISUAL_SCALE
        motor_b_angle += frame["db"] * MOTOR_VISUAL_SCALE

        trail.append((magnet_x, magnet_y))

        if len(trail) > 1600:
            trail.pop(0)

        corexy_index += 1


# ============================================================
# DRAWING HELPERS
# ============================================================

def draw_text(screen, font, text, x, y, color=TEXT):
    surface = font.render(text, True, color)
    screen.blit(surface, (x, y))


def draw_motor(screen, center, angle, label, steps, small_font):
    cx, cy = center

    pygame.draw.circle(screen, MOTOR, (int(cx), int(cy)), 34)
    pygame.draw.circle(screen, TEXT, (int(cx), int(cy)), 34, 2)
    pygame.draw.circle(screen, BG, (int(cx), int(cy)), 8)

    spoke_len = 28
    x2 = cx + math.cos(angle) * spoke_len
    y2 = cy + math.sin(angle) * spoke_len

    pygame.draw.line(screen, BG, (cx, cy), (x2, y2), 4)

    for i in range(4):
        a = angle + i * math.pi / 2
        x = cx + math.cos(a) * 22
        y = cy + math.sin(a) * 22
        pygame.draw.circle(screen, BG, (int(x), int(y)), 3)

    label_surface = small_font.render(label, True, TEXT)
    rect = label_surface.get_rect(center=(cx, cy + 50))
    screen.blit(label_surface, rect)

    step_surface = small_font.render(str(steps), True, MUTED)
    step_rect = step_surface.get_rect(center=(cx, cy + 70))
    screen.blit(step_surface, step_rect)


def draw_pulley(screen, center):
    cx, cy = center

    pygame.draw.circle(screen, PULLEY, (int(cx), int(cy)), 16)
    pygame.draw.circle(screen, TEXT, (int(cx), int(cy)), 16, 2)


def draw_belt(screen, p1, p2, width=3):
    pygame.draw.line(screen, BELT, p1, p2, width)


# ============================================================
# DRAW MECHANISM UNDER BOARD
# ============================================================

def draw_corexy_under_board(screen, small_font):
    left = BOARD_X
    right = BOARD_X + BOARD_SIZE
    top = BOARD_Y
    bottom = BOARD_Y + BOARD_SIZE

    # Slight shadow/depth area behind board
    pygame.draw.rect(
        screen,
        (18, 18, 28),
        (left - 20, top - 20, BOARD_SIZE + 40, BOARD_SIZE + 40),
        border_radius=18,
    )

    # Frame rails
    pygame.draw.rect(screen, RAIL, (left, top, BOARD_SIZE, BOARD_SIZE), 5)

    # Motor placement, visually under the board
    motor_a = (left + 58, bottom + 48)
    motor_b = (right - 58, bottom + 48)

    pulley_tl = (left, top)
    pulley_tr = (right, top)
    pulley_bl = (left, bottom)
    pulley_br = (right, bottom)

    carriage = (magnet_x, magnet_y)

    # Linear rails through carriage
    pygame.draw.line(screen, RAIL, (left, magnet_y), (right, magnet_y), 2)
    pygame.draw.line(screen, RAIL, (magnet_x, top), (magnet_x, bottom), 2)

    # Belts are drawn first so board can appear above them
    if show_belts:
        # Schematic CoreXY belt loops
        draw_belt(screen, motor_a, pulley_tl)
        draw_belt(screen, pulley_tl, carriage)
        draw_belt(screen, carriage, pulley_br)
        draw_belt(screen, pulley_br, motor_a)

        draw_belt(screen, motor_b, pulley_tr)
        draw_belt(screen, pulley_tr, carriage)
        draw_belt(screen, carriage, pulley_bl)
        draw_belt(screen, pulley_bl, motor_b)

    # Pulleys
    draw_pulley(screen, pulley_tl)
    draw_pulley(screen, pulley_tr)
    draw_pulley(screen, pulley_bl)
    draw_pulley(screen, pulley_br)

    # Planned path under transparent board
    if show_path and len(path_points) > 1:
        pygame.draw.lines(screen, TARGET, False, path_points, 2)

    for point in path_points:
        pygame.draw.circle(screen, TARGET, (int(point[0]), int(point[1])), 5)

    # Actual carriage trail
    if len(trail) > 1:
        pygame.draw.lines(screen, PATH, False, trail, 3)

    # Carriage under board
    pygame.draw.rect(
        screen,
        CARRIAGE,
        (magnet_x - 30, magnet_y - 24, 60, 48),
        border_radius=8,
    )

    pygame.draw.circle(screen, MAGNET, (int(magnet_x), int(magnet_y)), 13)
    pygame.draw.circle(screen, TEXT, (int(magnet_x), int(magnet_y)), 13, 2)

    # Motors outside/under edge
    draw_motor(screen, motor_a, motor_a_angle, "Motor A", motor_a_steps, small_font)
    draw_motor(screen, motor_b, motor_b_angle, "Motor B", motor_b_steps, small_font)

    # Current motor direction labels
    if moving and 0 <= corexy_index < len(corexy_frames):
        frame = corexy_frames[corexy_index]

        da = frame["da"]
        db = frame["db"]

        if da > 0:
            a_text = "A +"
            a_color = TARGET
        elif da < 0:
            a_text = "A -"
            a_color = WARNING
        else:
            a_text = "A 0"
            a_color = MUTED

        if db > 0:
            b_text = "B +"
            b_color = TARGET
        elif db < 0:
            b_text = "B -"
            b_color = WARNING
        else:
            b_text = "B 0"
            b_color = MUTED

        draw_text(screen, small_font, a_text, left + 130, bottom + 38, a_color)
        draw_text(screen, small_font, b_text, right - 165, bottom + 38, b_color)

    # CoreXY math readout
    readout_y = bottom + 100

    lx, ly = screen_to_local(magnet_x, magnet_y)

    draw_text(screen, small_font, "CoreXY: A = X + Y,  B = X - Y", left, readout_y, MUTED)
    draw_text(
        screen,
        small_font,
        f"Carriage local X={round(lx, 1)}  Y={round(ly, 1)}    A={motor_a_steps}  B={motor_b_steps}",
        left,
        readout_y + 24,
        TEXT,
    )


# ============================================================
# DRAW TRANSPARENT CHESSBOARD OVER MECHANISM
# ============================================================

def draw_transparent_chessboard(screen, font, small_font):
    board_surface = pygame.Surface((BOARD_SIZE, BOARD_SIZE), pygame.SRCALPHA)

    # Transparent squares
    for display_rank in range(8):
        for file_index in range(8):
            x = file_index * SQUARE_SIZE
            y = display_rank * SQUARE_SIZE

            color = LIGHT if (file_index + display_rank) % 2 == 0 else DARK

            pygame.draw.rect(board_surface, color, (x, y, SQUARE_SIZE, SQUARE_SIZE))
            pygame.draw.rect(board_surface, GRID, (x, y, SQUARE_SIZE, SQUARE_SIZE), 1)

    # Slight glass border
    pygame.draw.rect(board_surface, (205, 214, 244, 170), (0, 0, BOARD_SIZE, BOARD_SIZE), 3)

    if show_board_overlay:
        screen.blit(board_surface, (BOARD_X, BOARD_Y))

    # Coordinates on top
    for i in range(8):
        file_label = chr(ord("a") + i)
        rank_label = str(i + 1)

        draw_text(
            screen,
            small_font,
            file_label,
            BOARD_X + i * SQUARE_SIZE + SQUARE_SIZE / 2 - 5,
            BOARD_Y + BOARD_SIZE + 8,
            TEXT,
        )

        draw_text(
            screen,
            small_font,
            rank_label,
            BOARD_X - 25,
            BOARD_Y + (7 - i) * SQUARE_SIZE + SQUARE_SIZE / 2 - 8,
            TEXT,
        )


def get_piece_letter(piece):
    if piece is None:
        return ""

    return piece.symbol().upper()


def draw_pieces_on_top(screen, font):
    for square in chess.SQUARES:
        piece = game_board.piece_at(square)

        if piece is None:
            continue

        x, y = square_to_pixel(square)

        if piece.color == chess.WHITE:
            piece_color = WHITE_PIECE
            text_color = BG
        else:
            piece_color = BLACK_PIECE
            text_color = WHITE_PIECE

        pygame.draw.circle(screen, piece_color, (int(x), int(y)), 24)
        pygame.draw.circle(screen, BG, (int(x), int(y)), 24, 2)

        label = font.render(get_piece_letter(piece), True, text_color)
        rect = label.get_rect(center=(x, y))
        screen.blit(label, rect)


# ============================================================
# DRAW UI
# ============================================================

def draw_ui(screen, font, small_font):
    screen.fill(BG)

    draw_text(
        screen,
        small_font,
        "Enter = submit, R = reset, B = belts, P = path, O = board overlay, Esc = quit",
        50,
        40,
        WARNING,
    )

    # Draw order matters:
    # 1. mechanism under the board
    # 2. semi-transparent board
    # 3. chess pieces on top
    draw_corexy_under_board(screen, small_font)
    draw_transparent_chessboard(screen, font, small_font)
    draw_pieces_on_top(screen, font)

    bottom_y = WINDOW_H - 78

    draw_text(screen, small_font, f"Command: {command_text}", 50, bottom_y, TEXT)
    draw_text(screen, small_font, status_text, 50, bottom_y + 24, TEXT)
    draw_text(screen, small_font, engine_status, 50, bottom_y + 48, TEXT)


# ============================================================
# MAIN LOOP
# ============================================================

def main():
    global command_text, show_belts, show_path, show_board_overlay
    global magnet_x, magnet_y
    global motor_a_steps, motor_b_steps, motor_a_angle, motor_b_angle

    pygame.init()

    screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
    pygame.display.set_caption("Transparent CoreXY Chessboard Simulator")

    clock = pygame.time.Clock()

    font = pygame.font.SysFont("arial", 26, bold=True)
    small_font = pygame.font.SysFont("arial", 17)

    magnet_x, magnet_y = square_to_pixel(chess.A1)

    motor_a_steps, motor_b_steps = xy_to_ab_screen(magnet_x, magnet_y)
    motor_a_angle = motor_a_steps * MOTOR_VISUAL_SCALE
    motor_b_angle = motor_b_steps * MOTOR_VISUAL_SCALE

    start_engine()

    running = True

    while running:
        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False

                elif event.key == pygame.K_RETURN:
                    if not moving and not waiting_for_engine:
                        handle_user_move(command_text)
                        command_text = ""

                elif event.key == pygame.K_BACKSPACE:
                    if not moving and not waiting_for_engine:
                        command_text = command_text[:-1]

                elif event.key == pygame.K_r:
                    reset_game()

                elif event.key == pygame.K_b:
                    show_belts = not show_belts

                elif event.key == pygame.K_p:
                    show_path = not show_path

                elif event.key == pygame.K_o:
                    show_board_overlay = not show_board_overlay

                else:
                    if not moving and not waiting_for_engine:
                        ch = event.unicode

                        if ch and ch.isprintable():
                            command_text += ch

        update_motion()
        check_engine_result()
        maybe_start_stockfish_turn()

        draw_ui(screen, font, small_font)
        pygame.display.flip()

    if engine is not None:
        engine.quit()

    pygame.quit()
    sys.exit()


if __name__ == "__main__":
    main()