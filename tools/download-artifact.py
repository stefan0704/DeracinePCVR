#!/usr/bin/env python3
"""
Downloads the latest successful 'shadps4-deracine-pcvr' artifact from GitHub Actions
and extracts 'shadps4.exe' to pc-vr/shadps4.exe.

Solves the Azure Blob Storage 401 error caused by forwarding the GitHub Authorization
header across HTTP 302 redirects.
"""

import subprocess
import urllib.request
import json
import os
import zipfile
import io
import sys

REPO = "stefan0704/DeracinePCVR"
ARTIFACT_NAME = "shadps4-deracine-pcvr"
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUTPUT_EXE = os.path.join(REPO_ROOT, "pc-vr", "shadps4.exe")

def log(msg):
    print(f"[download-artifact] {msg}", flush=True)

def get_token():
    try:
        p = subprocess.run(["git", "credential", "fill"], input="protocol=https\nhost=github.com\n", capture_output=True, text=True)
        for line in p.stdout.splitlines():
            if line.startswith("password="):
                return line.split("=", 1)[1].strip()
    except Exception as e:
        log(f"Warning: Could not fetch git credential: {e}")
    return os.environ.get("GITHUB_TOKEN", None)

def main():
    target_run_id = sys.argv[1] if len(sys.argv) > 1 else None
    token = get_token()

    headers = {
        "User-Agent": "Mozilla/5.0",
        "Accept": "application/vnd.github+json"
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"

    if not target_run_id:
        log(f"Fetching latest runs for {REPO}...")
        runs_url = f"https://api.github.com/repos/{REPO}/actions/runs?per_page=10"
        req = urllib.request.Request(runs_url, headers=headers)
        with urllib.request.urlopen(req) as resp:
            runs_data = json.loads(resp.read().decode())
        
        for r in runs_data.get("workflow_runs", []):
            if r.get("status") == "completed" and r.get("conclusion") == "success":
                target_run_id = r.get("id")
                log(f"Found latest successful run: ID {target_run_id} (branch: {r.get('head_branch')})")
                break

    if not target_run_id:
        log("Error: No successful run found.")
        sys.exit(1)

    log(f"Fetching artifacts for Run {target_run_id}...")
    art_url = f"https://api.github.com/repos/{REPO}/actions/runs/{target_run_id}/artifacts"
    req_art = urllib.request.Request(art_url, headers=headers)
    with urllib.request.urlopen(req_art) as resp:
        art_data = json.loads(resp.read().decode())

    download_url = None
    for art in art_data.get("artifacts", []):
        if art["name"] == ARTIFACT_NAME:
            download_url = art["archive_download_url"]
            log(f"Found artifact: {art['name']}, ID: {art['id']}, size: {art['size_in_bytes']} bytes")
            break

    if not download_url:
        log(f"Error: Artifact '{ARTIFACT_NAME}' not found in run {target_run_id}.")
        sys.exit(1)

    log("Resolving Azure download redirect without forwarding GitHub Authorization header...")
    class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None

    opener = urllib.request.build_opener(NoRedirectHandler)
    azure_url = None
    try:
        req = urllib.request.Request(download_url, headers=headers)
        opener.open(req)
    except urllib.error.HTTPError as e:
        if e.code in (301, 302, 303, 307):
            azure_url = e.headers.get("Location")
        else:
            raise

    if not azure_url:
        log("Error: Failed to obtain Azure redirect location.")
        sys.exit(1)

    log(f"Downloading artifact ({azure_url[:60]}...)...")
    req_azure = urllib.request.Request(azure_url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req_azure) as resp:
        zip_bytes = resp.read()

    log(f"Downloaded {len(zip_bytes)} bytes. Extracting to {OUTPUT_EXE}...")
    extracted = False
    with zipfile.ZipFile(io.BytesIO(zip_bytes)) as z:
        for name in z.namelist():
            if name.lower().endswith("shadps4.exe"):
                with z.open(name) as src, open(OUTPUT_EXE, "wb") as dst:
                    dst.write(src.read())
                extracted = True
                break

    if extracted and os.path.exists(OUTPUT_EXE):
        size_mb = os.path.getsize(OUTPUT_EXE) / (1024 * 1024)
        log(f"SUCCESS: {OUTPUT_EXE} updated successfully ({size_mb:.2f} MB).")
    else:
        log("Error: Failed to extract shadps4.exe.")
        sys.exit(1)

if __name__ == "__main__":
    main()
