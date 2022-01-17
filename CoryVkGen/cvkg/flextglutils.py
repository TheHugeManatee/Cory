# Code in here is borrowed from flextGL https://github.com/mosra/flextgl

# Copyright © 2011–2018 Thomas Weber
# Copyright © 2014, 2015, 2016, 2018 Vladimír Vondruš <mosra@centrum.cz>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import xml.etree.ElementTree as ET

def extract_text(node: ET.Element, substitutes={'comment':''}) -> str:
    """originally 'xml_extract_all_text' in flextGL"""

    fragments = []
    if node.text: fragments += [node.text]
    for item in list(node):
        if item.tag in substitutes:
            fragments += [substitutes[item.tag]]
        elif item.text:
            # Sometimes <type> and <name> is not separated with a space in
            # vk.xml, add it here
            if fragments and fragments[-1] and fragments[-1][-1].isalnum():
                if fragments[-1][-1].isalnum() and item.text and item.text[0].isalnum():
                    fragments += [' ']
            fragments += [item.text]
        if item.tail: fragments.append(item.tail)
    return ''.join(fragments).strip()