#! /bin/bash

set_non_numa_pages()
{
        clear_huge_pages

        echo ""
        echo "  Input the number of 2MB pages"
        echo "  Example: to have 128MB of hugepages available, enter '64' to"
        echo "  reserve 64 * 2MB pages"
        echo -n "Number of pages: "
        read Pages

        echo "echo $Pages > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages" > .echo_tmp

        echo "Reserving hugepages"
        sudo sh .echo_tmp
        rm -f .echo_tmp

        create_mnt_huge
}

#
# Removes all reserved hugepages.
#
clear_huge_pages()
{
        echo > .echo_tmp
        for d in /sys/devices/system/node/node? ; do
                echo "echo 0 > $d/hugepages/hugepages-2048kB/nr_hugepages" >> .echo_tmp
        done
        echo "Removing currently reserved hugepages"
        sudo sh .echo_tmp
        rm -f .echo_tmp

        remove_mnt_huge
}

#
# Creates hugepage filesystem.
#
create_mnt_huge()
{
        echo "Creating /mnt/huge and mounting as hugetlbfs"
        sudo mkdir -p /mnt/huge

        grep -s '/mnt/huge' /proc/mounts > /dev/null
        if [ $? -ne 0 ] ; then
                sudo mount -t hugetlbfs nodev /mnt/huge
        fi
}

#
# Removes hugepage filesystem.
#
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

set_non_numa_pages
