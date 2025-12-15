import sys
from pathlib import Path

def load_data(path):
    data = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) != 3:
                continue
            size = int(parts[0])
            workers = int(parts[1])
            speedup = float(parts[2])
            if size not in data:
                data[size] = []
            data[size].append((workers, speedup))
    return data

def make_svg(path, data):
    width = 700
    height = 500
    margin = 60

    all_workers = set()
    max_speedup = 0
    for size, points in data.items():
        for w, s in points:
            all_workers.add(w)
            if s > max_speedup:
                max_speedup = s

    workers = sorted(all_workers)
    max_w = max(workers)
    ideal_speedup = max_w
    if ideal_speedup > max_speedup:
        max_speedup = ideal_speedup

    colors = ["#d62728", "#1f77b4", "#2ca02c", "#ff7f0e", "#9467bd"]

    items = []
    items.append(f'<line x1="{margin}" y1="{height - margin}" x2="{width - margin}" y2="{height - margin}" stroke="black" stroke-width="2"/>')
    items.append(f'<line x1="{margin}" y1="{margin}" x2="{margin}" y2="{height - margin}" stroke="black" stroke-width="2"/>')

    def x_pos(w):
        return margin + (w - workers[0]) / (max_w - workers[0]) * (width - 2 * margin)

    def y_pos(s):
        return height - margin - s / max_speedup * (height - 2 * margin)

    for w in workers:
        x = x_pos(w)
        items.append(f'<line x1="{x}" y1="{height - margin}" x2="{x}" y2="{height - margin + 6}" stroke="black"/>')
        items.append(f'<text x="{x}" y="{height - margin + 20}" font-size="12" text-anchor="middle">{w}</text>')

    step = max(1, int(max_speedup / 5))
    for val in range(0, int(max_speedup) + step, step):
        y = y_pos(val)
        items.append(f'<line x1="{margin - 6}" y1="{y}" x2="{margin}" y2="{y}" stroke="black"/>')
        items.append(f'<text x="{margin - 10}" y="{y + 4}" font-size="12" text-anchor="end">{val}</text>')

    ideal_points = " ".join(f"{x_pos(w)},{y_pos(w)}" for w in workers)
    items.append(f'<polyline points="{ideal_points}" fill="none" stroke="gray" stroke-width="2" stroke-dasharray="6,4"/>')

    sizes = sorted(data.keys())
    for idx, size in enumerate(sizes):
        color = colors[idx % len(colors)]
        points = sorted(data[size])
        pts = " ".join(f"{x_pos(w)},{y_pos(s)}" for w, s in points)
        items.append(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="2"/>')
        for w, s in points:
            items.append(f'<circle cx="{x_pos(w)}" cy="{y_pos(s)}" r="4" fill="{color}"/>')

    items.append(f'<text x="{width / 2}" y="{height - 15}" font-size="14" text-anchor="middle">N (workers)</text>')
    items.append(f'<text x="15" y="{height / 2}" font-size="14" text-anchor="middle" transform="rotate(-90 15 {height / 2})">Speedup</text>')

    legend_x = width - margin - 100
    legend_y = margin + 20
    items.append(f'<line x1="{legend_x}" y1="{legend_y}" x2="{legend_x + 20}" y2="{legend_y}" stroke="gray" stroke-width="2" stroke-dasharray="6,4"/>')
    items.append(f'<text x="{legend_x + 25}" y="{legend_y + 4}" font-size="12">ideal</text>')
    for idx, size in enumerate(sizes):
        color = colors[idx % len(colors)]
        ly = legend_y + 20 * (idx + 1)
        items.append(f'<line x1="{legend_x}" y1="{ly}" x2="{legend_x + 20}" y2="{ly}" stroke="{color}" stroke-width="2"/>')
        items.append(f'<text x="{legend_x + 25}" y="{ly + 4}" font-size="12">M={size}</text>')

    with open(path, "w", encoding="utf-8") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">')
        for item in items:
            f.write(item)
        f.write("</svg>")

def main():
    if len(sys.argv) < 3:
        print("usage: plot.py RESULTS OUTPUT", file=sys.stderr)
        return 1
    results = Path(sys.argv[1])
    output = Path(sys.argv[2])
    data = load_data(results)
    make_svg(output, data)
    return 0

if __name__ == "__main__":
    sys.exit(main())
