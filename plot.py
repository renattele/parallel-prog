import math
import sys
from pathlib import Path

def load_table(path):
    sizes = []
    scalar = []
    vector = []
    with open(path, "r", encoding="utf-8") as file:
        for line in file:
            parts = line.strip().split()
            if len(parts) != 3:
                continue
            sizes.append(int(parts[0]))
            scalar.append(float(parts[1]))
            vector.append(float(parts[2]))
    return sizes, scalar, vector

def scale(values, start, end):
    low = min(values)
    high = max(values)
    span = high - low
    if span == 0:
        return [start + (end - start) / 2] * len(values)
    return [start + (value - low) * (end - start) / span for value in values]

def make_svg(path, sizes, scalar, vector, title):
    width = 800
    height = 600
    margin = 60
    max_y = max(max(scalar), max(vector))
    xs = scale(list(range(len(sizes))), margin, width - margin)
    ys_scalar = [height - margin - value / max_y * (height - 2 * margin) for value in scalar]
    ys_vector = [height - margin - value / max_y * (height - 2 * margin) for value in vector]
    items = []
    items.append(f'<line x1="{margin}" y1="{height - margin}" x2="{width - margin}" y2="{height - margin}" stroke="black" stroke-width="2"/>')
    items.append(f'<line x1="{margin}" y1="{margin}" x2="{margin}" y2="{height - margin}" stroke="black" stroke-width="2"/>')
    for i, size in enumerate(sizes):
        x = xs[i]
        items.append(f'<line x1="{x}" y1="{height - margin}" x2="{x}" y2="{height - margin + 6}" stroke="black"/>')
        items.append(f'<text x="{x}" y="{height - margin + 20}" font-size="14" text-anchor="middle">{size}</text>')
    step = max(1, int(math.ceil(max_y / 5)))
    for value in range(0, int(math.ceil(max_y)) + 1, step):
        y = height - margin - value / max_y * (height - 2 * margin)
        items.append(f'<line x1="{margin - 6}" y1="{y}" x2="{margin}" y2="{y}" stroke="black"/>')
        items.append(f'<text x="{margin - 10}" y="{y + 5}" font-size="14" text-anchor="end">{value}</text>')
    def polyline(points, color):
        text = " ".join(f"{xs[i]},{points[i]}" for i in range(len(points)))
        items.append(f'<polyline points="{text}" fill="none" stroke="{color}" stroke-width="2"/>')
        for i in range(len(points)):
            items.append(f'<circle cx="{xs[i]}" cy="{points[i]}" r="4" fill="{color}"/>')
    polyline(ys_scalar, "#d62728")
    polyline(ys_vector, "#1f77b4")
    items.append(f'<text x="{margin}" y="{margin - 20}" font-size="16">{title}</text>')
    items.append(f'<text x="{width - margin}" y="{margin - 20}" font-size="14" text-anchor="end">Scalar vs AVX2</text>')
    with open(path, "w", encoding="utf-8") as file:
        file.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">')
        for item in items:
            file.write(item)
        file.write("</svg>")

def main():
    if len(sys.argv) < 4:
        print("usage: plot.py RESULTS OUTPUT TITLE...", file=sys.stderr)
        return 1
    results = Path(sys.argv[1])
    output = Path(sys.argv[2])
    title = " ".join(sys.argv[3:])
    sizes, scalar, vector = load_table(results)
    make_svg(output, sizes, scalar, vector, title)
    return 0

if __name__ == "__main__":
    sys.exit(main())
