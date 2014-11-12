/usr/local/bin/qemu-system-x86_64 -m 2048 -hda ./hdd.img -drive file=./nvme.img,if=none,id=D22 -device nvme,drive=D22,serial=1234 --enable-kvm -redir tcp:2222::22 -smb /home/SISA/d.waddington
