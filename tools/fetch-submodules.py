import os
import subprocess
import configparser

def fetch_submodules():
    cfg = configparser.ConfigParser()
    gitmodules_path = os.path.join('src', '.gitmodules')
    if not os.path.exists(gitmodules_path):
        print(f"File not found: {gitmodules_path}")
        return
    cfg.read(gitmodules_path)
    for s in cfg.sections():
        if 'path' in cfg[s] and 'url' in cfg[s]:
            subpath = cfg[s]['path'].replace('\\', '/')
            full_path = os.path.join('src', subpath)
            url = cfg[s]['url']
            if not os.path.exists(full_path) or not os.listdir(full_path):
                print(f"Fetching {subpath} from {url} ...")
                subprocess.run(['git', 'clone', '--depth', '1', url, full_path], check=True)
            else:
                print(f"Submodule already present: {subpath}")

if __name__ == '__main__':
    fetch_submodules()
