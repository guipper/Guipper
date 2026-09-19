#!/usr/bin/env python3
"""Exercise the production Windows backend and real WinSparkle signatures locally.

The probe is built with GUIPPER_UPDATE_TEST, an older version, and the public key.
Only that test binary permits a loopback appcast override. No installer is run.
"""
import argparse,functools,hashlib,http.server,json,os,pathlib,subprocess,tempfile,threading
import xml.etree.ElementTree as ET

NS='http://www.andymatuschak.org/xml-namespaces/sparkle'
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['probe','installer','tool','private-key','output']:
        parser.add_argument('--'+name,type=pathlib.Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    signed=subprocess.check_output([str(args.tool),'sign','--private-key-file',str(args.private_key),str(args.installer)],text=True).strip()
    payload=args.installer.read_bytes()
    results={}
    with tempfile.TemporaryDirectory(prefix='guipper-windows-updates-') as temp:
        folder=pathlib.Path(temp)
        class Handler(http.server.SimpleHTTPRequestHandler):
            def log_message(self,*args):pass
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=folder))
        threading.Thread(target=server.serve_forever,daemon=True).start()
        base=f'http://127.0.0.1:{server.server_port}'
        try:
            for case in ['valid','tampered','unsigned','wrong-signature','offline','cancel']:
                root=ET.Element('rss',version='2.0');channel=ET.SubElement(root,'channel')
                ET.SubElement(channel,'title').text='Guipper local update regression'
                item=ET.SubElement(channel,'item');ET.SubElement(item,'title').text='Guipper 0.1.0-beta.6'
                attributes={'url':base+'/installer.exe','length':str(len(payload)),'type':'application/octet-stream','{'+NS+'}version':'0.1.0-beta.6'}
                if case!='unsigned':attributes['{'+NS+'}edSignature']=signed if case!='wrong-signature' else ('A'*86+'==')
                ET.SubElement(item,'enclosure',attributes)
                (folder/'appcast.xml').write_bytes(ET.tostring(root,encoding='utf-8',xml_declaration=True))
                (folder/'installer.exe').write_bytes(payload if case!='tampered' else payload[:100]+bytes([payload[100]^1])+payload[101:])
                cache=folder/case
                env=dict(os.environ,GUIPPER_TEST_APPCAST=base+'/appcast.xml')
                if case=='offline':env['GUIPPER_TEST_APPCAST']='http://127.0.0.1:1/appcast.xml'
                log=args.output/(case+'.log')
                with log.open('w') as stream:
                    run=subprocess.run([str(args.probe.resolve()),str(cache),'cancel' if case=='cancel' else 'download'],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=75)
                staged=cache/'Guipper-update.exe'
                if case=='valid':
                    ok=run.returncode==0 and staged.is_file() and hashlib.sha256(staged.read_bytes()).digest()==hashlib.sha256(payload).digest()
                    if ok:(args.output/'verified-installer.exe').write_bytes(staged.read_bytes())
                elif case=='cancel':ok=run.returncode==0 and not staged.exists()
                else:ok=run.returncode==3 and not staged.exists()
                results[case]={'passed':ok,'exit':run.returncode}
                print(case,results[case],flush=True)
                if not ok:break
        finally:server.shutdown()
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    return 0 if len(results)==6 and all(r['passed'] for r in results.values()) else 1
if __name__=='__main__':raise SystemExit(main())
