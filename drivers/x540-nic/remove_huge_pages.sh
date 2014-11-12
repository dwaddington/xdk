#! /bin/bash

remove_mnt_huge()
{
        echo "Unmounting /mnt/huge and removing directory"
        grep -s '/mnt/huge' /proc/mounts > /dev/null
        if [ $? -eq 0 ] ; then
                sudo umount /mnt/huge
        fi

        if [ -d /mnt/huge ] ; then
                sudo rm -R /mnt/huge
        fi
}

remove_mnt_huge
