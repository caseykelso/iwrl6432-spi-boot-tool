#
#  Copyright (C) 2023 Texas Instruments Incorporated
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#   Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#    Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the
#    distribution.
#
#    Neither the name of Texas Instruments Incorporated nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS3  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Python script for converting appimage into hexadecimal format. This script 
# will generate a file which will contain an array named as image and a 
# variable named as Size. Image stores the appimage in hexadecimal format 
# which is transmitted via SPI and Size tells us about the size of an image array.

import binascii
import os
import sys

# appimage filename which we want to transmit via SPI
appimage_filename=sys.argv[1]


with open(appimage_filename, 'rb') as f:
    content = f.read()
output=binascii.hexlify(content)
file = open('appimage.txt','wb')
file.write(output)

# Creating header file image_data for storing appimage binary data in hexadecimal format into an array
file1=open('appimagedata.h','w')
file1.write("uint8_t image []={")

Size=1
with open('appimage.txt') as f:
    file1.write('0x')
    a=f.read(2)
    f.seek(2)
    file1.write(a) 
    while True:
         
        # Read from file
        c = f.read(2)
        if not c:
            break
 
        # save the character into image array
        file1.write(',0x'+c)
        Size=Size+1

file1.write("};\n")

# Writing size of the array into file
file1.write("uint32_t Size="+str(Size)+";")





