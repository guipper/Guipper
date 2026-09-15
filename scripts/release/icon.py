#!/usr/bin/env python3
"""Embed the unchanged raster logo in a standard-size SVG icon container."""
import base64
import sys
from pathlib import Path
source,target=map(Path,sys.argv[1:])
encoded=base64.b64encode(source.read_bytes()).decode('ascii')
target.write_text('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="512" height="512" viewBox="0 0 512 512"><image width="512" height="512" preserveAspectRatio="xMidYMid meet" xlink:href="data:image/png;base64,'+encoded+'"/></svg>\n')
