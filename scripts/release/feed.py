#!/usr/bin/env python3
"""Generate a Windows appcast from already uploaded, signed release artifacts.
This writes a local file; publishing the channel is an explicit separate step.
"""
import argparse
import base64
import json
import xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import urlparse

SPARKLE='http://www.andymatuschak.org/xml-namespaces/sparkle'
def generate(item):
    for field in ['url','notes_url']:
        value=urlparse(item[field])
        if value.scheme!='https' or not value.hostname or value.username or value.password:
            raise ValueError('HTTPS URLs without credentials are required')
    if len(base64.b64decode(item['signature'],validate=True))!=64:
        raise ValueError('An Ed25519 signature is required')
    if not isinstance(item['size'],int) or item['size']<=0: raise ValueError('Invalid artifact size')
    ET.register_namespace('sparkle',SPARKLE)
    rss=ET.Element('rss',version='2.0'); channel=ET.SubElement(rss,'channel')
    ET.SubElement(channel,'title').text='Guipper updates'
    release=ET.SubElement(channel,'item');ET.SubElement(release,'title').text='Guipper '+item['version']
    ET.SubElement(release,'{'+SPARKLE+'}releaseNotesLink').text=item['notes_url']
    ET.SubElement(release,'enclosure',{
        'url':item['url'],'length':str(item['size']),'type':'application/octet-stream',
        '{'+SPARKLE+'}version':item['version'],'{'+SPARKLE+'}edSignature':item['signature']})
    return ET.tostring(rss,encoding='utf-8',xml_declaration=True)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('manifest',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();args.output.write_bytes(generate(json.loads(args.manifest.read_text())))
