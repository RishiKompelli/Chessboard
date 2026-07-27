#!/usr/bin/env python3

from datetime import datetime, timezone
from pathlib import Path
import subprocess
import sys


def get_commit_lines():
    command = [
        "git",
        "log",
        "--since=6 hours ago",
        "--date=iso",
        "--pretty=format:%h\t%ad\t%s",
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    return lines


def get_remote_url():
    try:
        result = subprocess.run(
            ["git", "remote", "get-url", "origin"],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None


def render_report(lines, generated_at, remote_url=None):
    parts = ["# Progress Report", "", f"Generated: {generated_at.isoformat(timespec='seconds')} UTC", "Reporting window: last 6 hours"]

    if remote_url:
        github_url = None
        if remote_url.startswith("git@github.com:"):
            github_url = remote_url.replace("git@github.com:", "https://github.com/")
        elif remote_url.endswith(".git"):
            github_url = remote_url[:-4]
        else:
            github_url = remote_url

        if github_url:
            parts.append(f"Repository: {github_url}")

    parts.append("")

    if not lines:
        parts.append("No new commits were made in the last 6 hours.")
    else:
        parts.append(f"Commits found: {len(lines)}")
        parts.append("")
        parts.append("| Commit | Date | Message |")
        parts.append("|---|---|---|")
        for line in lines:
            parts.append("| " + " | ".join(line.split("\t")) + " |")

    parts.append("")
    parts.append("This report is generated automatically every 6 hours by GitHub Actions.")
    return "\n".join(parts)


def main():
    repo_root = Path(__file__).resolve().parent.parent
    report_dir = repo_root / "progress-reports"
    report_dir.mkdir(parents=True, exist_ok=True)

    lines = get_commit_lines()
    generated_at = datetime.now(timezone.utc)
    remote_url = get_remote_url()
    report = render_report(lines, generated_at, remote_url)

    latest_path = report_dir / "latest.md"
    latest_path.write_text(report, encoding="utf-8")
    print(report)

    return 0


if __name__ == "__main__":
    sys.exit(main())
