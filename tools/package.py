from pathlib import Path
import argparse,subprocess
project=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('build',type=Path);a=p.parse_args()
build=a.build.resolve();stage=build/'stage';rpm=build/'rpm';(rpm/'SPECS').mkdir(parents=True,exist_ok=True)
subprocess.run(['cmake','--install',str(build),'--prefix','/usr/local'],env={**__import__('os').environ,'DESTDIR':str(stage)},check=True)
spec=f'''%global _build_id_links none
%global debug_package %{{nil}}
Name: domed-dialogs
Version: 4.2.2
Release: 1.local
Summary: Native FLTK dialogs for Zenity command-line clients
License: GPL-3.0-or-later
Provides: zenity = %{{version}}
Obsoletes: zenity < 5
%description
Toolkit-independent replacement for the Zenity operations used by Winetricks:
messages, questions, input, passwords, radio/check lists, forms, files and progress.
Unknown operations fail explicitly. The GTK Zenity application is not required.
%prep
%build
%install
mkdir -p %{{buildroot}}/usr/bin %{{buildroot}}/usr/local/bin %{{buildroot}}/usr/share/licenses/domed-dialogs
install -m755 {stage}/usr/local/bin/dome-dialog %{{buildroot}}/usr/bin/zenity
ln -s /usr/bin/zenity %{{buildroot}}/usr/local/bin/dome-dialog
cp {project}/LICENSE %{{buildroot}}/usr/share/licenses/domed-dialogs/LICENSE
%files
/usr/bin/zenity
/usr/local/bin/dome-dialog
%license /usr/share/licenses/domed-dialogs/LICENSE
'''
(rpm/'SPECS/domed-dialogs.spec').write_text(spec)
subprocess.run(['rpmbuild','-bb','--define',f'_topdir {rpm}',str(rpm/'SPECS/domed-dialogs.spec')],check=True,env={**__import__('os').environ,'QA_RPATHS':'0x0002'})
