import chess
import chess.engine

STOCKFISH_PATH = r"C:\Users\rishi_7b7n0gh\Downloads\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2.exe"

board = chess.Board()

engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)

result = engine.play(board, chess.engine.Limit(time=0.5))

print("Stockfish move:", result.move)

engine.quit()