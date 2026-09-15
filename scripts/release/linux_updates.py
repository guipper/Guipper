"""Validation shared by update configuration and signed packaging."""
import re
from urllib.parse import urlparse

def https_url(value):
    parsed=urlparse(value)
    if (parsed.scheme!='https' or not parsed.hostname or parsed.username or parsed.password
            or parsed.fragment or any(c.isspace() for c in value)):
        raise ValueError('Expected an HTTPS URL without credentials, whitespace or fragments')
    return value

def update_information(value):
    fields=value.split('|')
    if len(fields)==2 and fields[0]=='zsync':
        https_url(fields[1])
    elif len(fields)==5 and fields[0]=='gh-releases-zsync':
        if not all(re.fullmatch(r'[A-Za-z0-9_.-]+',part) for part in fields[1:4]):
            raise ValueError('Invalid GitHub owner, repository or channel tag')
        if fields[3]=='latest':
            raise ValueError('Use explicit stable/beta channel tags, not latest')
        if not re.fullmatch(r'[A-Za-z0-9_.*-]+\.AppImage\.zsync',fields[4]):
            raise ValueError('Expected an AppImage .zsync asset pattern')
    else:
        raise ValueError('Use gh-releases-zsync|owner|repo|channel|asset.AppImage.zsync or zsync|https://...')
    return value

def fingerprint(value):
    if not re.fullmatch(r'[A-Fa-f0-9]{40}',value):
        raise ValueError('Expected the full 40-character OpenPGP signing fingerprint')
    return value.upper()
