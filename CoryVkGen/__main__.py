import os.path
import time
import urllib.request
import argparse

from cvkg import SpecParser, Generator

vk_spec_url = 'https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/{}/xml/vk.xml'


################################################################################
# Spec file download
################################################################################


def file_age(filename):
    return (time.time() - os.path.getmtime(filename)) / 3600.0


def download_spec(version, spec_dir, save_as, always_download=False):
    download_url = vk_spec_url.format(version)
    if not os.path.exists(save_as):
        os.makedirs(save_as)

    spec_file = os.path.join(spec_dir, save_as)

    if always_download or not os.path.exists(spec_file) or file_age(spec_file) > 3 * 24:
        print('Downloading %s' % download_url)
        urllib.request.urlretrieve(download_url, spec_file)
    else:
        print(f"File already downloaded under {spec_file}")
    return spec_file


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generator for the Cory Vulkan wrapper library')
    parser.add_argument('--registry', help="Path to vulkan registry xml")
    parser.add_argument('--output_folder', help="Where to put the generated cvk headers.")

    args = parser.parse_args()

    if not args.output_folder:
        print("No output folder specified!")
    if not args.registry:
        print("No path to vulkan registry specified - downloading registry xml file...")

        spec_version = 'v1.2.203'
        registry_file_path = download_spec(spec_version, "c:/tmp/", "vk.v1.2.203.xml")
    else:
        registry_file_path = args.registry

    ############################################
    ##               PARSING                  ##
    ############################################
    parser = SpecParser(registry_file_path)

    print("## Vulkan Types")
    #    list a summary of the categories
    cats = {}
    for type in parser.types.values():
        cat = type.category if type.category != '' else '<no cat>'
        if cat not in cats:
            cats[cat] = 1
        else:
            cats[cat] += 1

    for c, v in cats.items():
        print(f"  - {c : <14}{v}")

    ############################################
    ##              GENERATION                ##
    ############################################
    module_dir = os.path.dirname(os.path.abspath(__file__))
    template_dir = os.path.join(module_dir, 'templates')
    generator = Generator(parser, template_dir=template_dir)
    generator.generate(args.output_folder)
