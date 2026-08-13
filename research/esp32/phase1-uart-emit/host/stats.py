"""
Phase 1 Telemetry Analyzer — Reads CSV log and computes scientific stats (min, max, mean, stddev) for notes.
Usage: python host/stats.py [--csv lux_telemetry.csv]
"""

import argparse
import csv
import math
import sys
from rich.console import Console
from rich.table import Table

def calculate_stats(data):
    if not data:
        return {"min": 0, "max": 0, "mean": 0, "std": 0}
    n = len(data)
    min_val = min(data)
    max_val = max(data)
    mean_val = sum(data) / n
    variance = sum((x - mean_val) ** 2 for x in data) / n if n > 1 else 0
    std_val = math.sqrt(variance)
    return {
        "min": min_val,
        "max": max_val,
        "mean": mean_val,
        "std": std_val,
        "count": n
    }

def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux — Telemetry Statistical Analyzer")
    parser.add_argument("--csv", default="lux_telemetry.csv", help="Input CSV log file")
    args = parser.parse_args()

    console = Console()
    console.print(f"[bold cyan]Pyintel Lux[/bold cyan] — Telemetry Statistical Analysis\n")

    try:
        with open(args.csv, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            rows = list(reader)
    except FileNotFoundError:
        console.print(f"[bold red]Error:[/bold red] File '{args.csv}' not found.")
        sys.exit(1)

    if not rows:
        console.print("[yellow]No data rows found in CSV.[/yellow]")
        return

    # Extract numeric columns (filtering out initial zero delay row)
    delays = [float(r["inter_frame_delay_ms"]) for r in rows if float(r["inter_frame_delay_ms"]) > 0]
    fps_vals = [float(r["instant_fps"]) for r in rows if float(r["instant_fps"]) > 0]
    esp_dts = [float(r["esp_ts_delta_us"]) for r in rows if float(r["esp_ts_delta_us"]) > 0]
    total_bytes = sum(int(r["frame_bytes"]) for r in rows)
    crc_passed = sum(1 for r in rows if r["crc_ok"].strip().lower() in ("true", "1", "ok"))
    crc_failed = len(rows) - crc_passed

    delay_stats = calculate_stats(delays)
    fps_stats = calculate_stats(fps_vals)
    esp_dt_stats = calculate_stats(esp_dts)

    # Group stats by symbol type / burst transition
    intra_burst_delays = []  # HB -> Counter (sub-millisecond transmit time)
    inter_burst_delays = []  # Counter -> HB (1-second sleep tick)
    
    for i in range(1, len(rows)):
        prev_sym = rows[i-1]["symbol_name"]
        curr_sym = rows[i]["symbol_name"]
        delay = float(rows[i]["inter_frame_delay_ms"])
        if prev_sym == "LUX_SYM_HEARTBEAT" and curr_sym == "APP_COUNTER":
            intra_burst_delays.append(delay)
        elif prev_sym == "APP_COUNTER" and curr_sym == "LUX_SYM_HEARTBEAT":
            inter_burst_delays.append(delay)

    # Handle case where capture started on APP_COUNTER instead of HEARTBEAT
    if not intra_burst_delays or (sum(intra_burst_delays)/len(intra_burst_delays) > 10.0):
        intra_burst_delays, inter_burst_delays = inter_burst_delays, intra_burst_delays

    burst_intra = calculate_stats(intra_burst_delays)
    burst_inter = calculate_stats(inter_burst_delays)

    table = Table(title=f"Telemetry Summary (File: {args.csv})")
    table.add_column("Metric", style="cyan", no_wrap=True)
    table.add_column("Count", style="white")
    table.add_column("Min", style="green")
    table.add_column("Max", style="red")
    table.add_column("Mean ± StdDev", style="yellow")

    table.add_row(
        "Overall Inter-frame Delay (ms)",
        str(delay_stats["count"]),
        f"{delay_stats['min']:.2f}",
        f"{delay_stats['max']:.2f}",
        f"{delay_stats['mean']:.2f} ± {delay_stats['std']:.2f}"
    )
    table.add_row(
        "  ├─ Intra-burst Delay (HB → Counter) (ms)",
        str(burst_intra["count"]),
        f"{burst_intra['min']:.2f}",
        f"{burst_intra['max']:.2f}",
        f"{burst_intra['mean']:.2f} ± {burst_intra['std']:.2f}"
    )
    table.add_row(
        "  └─ Inter-burst Delay (Counter → HB) (ms)",
        str(burst_inter["count"]),
        f"{burst_inter['min']:.2f}",
        f"{burst_inter['max']:.2f}",
        f"{burst_inter['mean']:.2f} ± {burst_inter['std']:.2f}"
    )
    table.add_row(
        "Instantaneous FPS",
        str(fps_stats["count"]),
        f"{fps_stats['min']:.2f}",
        f"{fps_stats['max']:.2f}",
        f"{fps_stats['mean']:.2f} ± {fps_stats['std']:.2f}"
    )
    table.add_row(
        "ESP32 Clock Delta (µs)",
        str(esp_dt_stats["count"]),
        f"{esp_dt_stats['min']:.0f}",
        f"{esp_dt_stats['max']:.0f}",
        f"{esp_dt_stats['mean']:.1f} ± {esp_dt_stats['std']:.1f}"
    )

    console.print(table)

    crc_pass_pct = (crc_passed / len(rows)) * 100 if rows else 0
    console.print(f"\n[bold]Total Packets Received:[/bold] {len(rows)}")
    console.print(f"[bold]Total Bytes Transferred:[/bold] {total_bytes} bytes")
    console.print(f"[bold]CRC Reliability Rate:[/bold] [green]{crc_passed}/{len(rows)} ({crc_pass_pct:.1f}% OK)[/green]")

    markdown_summary = f"""
### 📊 Benchmark Summary ({args.csv})
- **Total Frames Captured:** {len(rows)} ({total_bytes} bytes)
- **Data Integrity:** {crc_passed}/{len(rows)} CRC frames passed ({crc_pass_pct:.1f}%)
- **Intra-burst Transmission Delay (HB → Counter):** Min = {burst_intra['min']:.2f} ms | Max = {burst_intra['max']:.2f} ms | Mean = {burst_intra['mean']:.2f} ± {burst_intra['std']:.2f} ms
- **Inter-burst Tick Delay (Counter → HB):** Min = {burst_inter['min']:.2f} ms | Max = {burst_inter['max']:.2f} ms | Mean = {burst_inter['mean']:.2f} ± {burst_inter['std']:.2f} ms
- **ESP32 Microsecond Clock Delta:** Min = {esp_dt_stats['min']:.0f} µs | Max = {esp_dt_stats['max']:.0f} µs | Mean = {esp_dt_stats['mean']:.1f} µs
"""
    console.print("\n[bold cyan]Markdown Summary for Notes:[/bold cyan]")
    console.print(markdown_summary)

if __name__ == "__main__":
    main()
