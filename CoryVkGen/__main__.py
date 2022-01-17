import os.path
import time
import urllib

from cvkg import SpecParser

vk_spec_url = 'https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/{}/xml/vk.xml'

################################################################################
# Spec file download
################################################################################

def file_age(filename):
    return (time.time() - os.path.getmtime(filename)) / 3600.0

def download_spec(version, spec_dir, save_as, always_download = False):
    download_url = vk_spec_url.format(version)
    if not os.path.exists(save_as):
        os.makedirs(save_as)

    spec_file = os.path.join(spec_dir, save_as)

    if always_download or not os.path.exists(spec_file) or file_age(spec_file) > 3*24:
        print ('Downloading %s' % download_url)
        urllib.request.urlretrieve(download_url, spec_file)
    else:
        print(f"File already downloaded under {spec_file}")
    return spec_file


if __name__ == '__main__':
    spec_version = 'v1.2.162'
    spec_file_path = download_spec(spec_version, "c:/tmp/", "vk.xml")

    parser = SpecParser.SpecParser(spec_file_path)


    print(parser.types['VkResult'])
    print(parser.types['VkFramebufferCreateFlagBits'])

    print(parser.commands['vkCreateInstance'])