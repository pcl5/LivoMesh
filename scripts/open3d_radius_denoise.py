#!/usr/bin/env python3
"""
Quick radius-outlier removal using Open3D.

Example:
    python scripts/open3d_radius_denoise.py \
        --input data/FAST_LIVO2/test/fast_livo_xiaojuchang.pcd \
        --output output/fast_livo_xiaojuchang_open3d.pcd \
        --radius 0.08 \
        --min-neighbors 8
"""

import argparse
import pathlib
import sys
import time

try:
    import open3d as o3d
except ImportError as exc:
    print("Open3D is required. Install with `pip install open3d`.", file=sys.stderr)
    raise


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Radius outlier removal using Open3D.")
    parser.add_argument(
        "--input",
        required=True,
        type=pathlib.Path,
        help="Input PCD/PLY file (any format supported by Open3D).",
    )
    parser.add_argument(
        "--output",
        required=True,
        type=pathlib.Path,
        help="Output path for the filtered point cloud.",
    )
    parser.add_argument(
        "--radius",
        type=float,
        default=0.08,
        help="Radius for neighborhood search (meters). Default: 0.08",
    )
    parser.add_argument(
        "--min-neighbors",
        type=int,
        default=6,
        help="Minimum number of neighbors required to keep a point. Default: 6",
    )
    parser.add_argument(
        "--keep-statistics",
        action="store_true",
        help="Save rejected points as <output>_rejected.pcd for inspection.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    start = time.time()

    if not args.input.exists():
        raise FileNotFoundError(f"Input file not found: {args.input}")

    pcd = o3d.io.read_point_cloud(str(args.input))
    if pcd.is_empty():
        raise RuntimeError(f"Failed to load any points from {args.input}")

    print(f"Loaded {len(pcd.points)} points from {args.input}")

    filtered, ind = pcd.remove_radius_outlier(
        nb_points=args.min_neighbors,
        radius=args.radius,
    )

    removed_count = len(pcd.points) - len(filtered.points)
    print(
        f"Filtered cloud has {len(filtered.points)} points "
        f"(removed {removed_count} / {len(pcd.points)})."
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not o3d.io.write_point_cloud(str(args.output), filtered):
        raise RuntimeError(f"Failed to write {args.output}")

    print(f"Filtered cloud saved to {args.output}")

    if args.keep_statistics:
        rejected = pcd.select_by_index(ind, invert=True)
        rejected_path = args.output.with_suffix(args.output.suffix + "_rejected.pcd")
        o3d.io.write_point_cloud(str(rejected_path), rejected)
        print(f"Rejected points saved to {rejected_path}")

    print(f"Elapsed time: {time.time() - start:.2f}s")


if __name__ == "__main__":
    main()
