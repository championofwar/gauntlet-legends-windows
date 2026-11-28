"""Exercise the two-cycle tile-alpha blend through real guest display-list commands."""
from pe_host import *
from struct import pack

ram=symbols['ram']
target=0x101000
dl=0x180000
first,second=0x181000,0x182000
background=0x003f
def sample(tile,second_alpha,weight,triangle):
    C.memmove(ram+target,pack('>H',background),2)
    C.memmove(ram+first,bytes((255,0,0,255)),4)
    C.memmove(ram+second,bytes((0,255,0,second_alpha)),4)
    commands=[(0xff10019f,target),(0xe3000a01,1<<20)]
    for index,bank,addr in ((tile,8,first),((tile+1)&7,16,second)):
        commands.extend([(0xfd180000,addr),(0xf5180200|bank,index<<24),
                         (0xf4000000,index<<24),(0xf2000000,index<<24)])
    commands.extend([(0xfc122bff,0xffff9238),(0xfb000000,weight)])
    if triangle:
        vertices=b''.join(pack('<10fI',x,y,0,1,0,0,0,0,0,2,0xffffffff)
                          for x,y in ((0,0),(3,0),(0,3)))
        C.memmove(symbols['vertices'],vertices,len(vertices))
        commands.extend([(0xd7000002|(tile<<8),0xffffffff),(0x05000204,0)])
    else:
        commands.extend([(0xe4004004,tile<<24),(0xe1000000,0),(0xf1000000,0)])
    commands.append((0xdf000000,0))
    data=b''.join(pack('>II',a,b) for a,b in commands)
    C.memmove(ram+dl,data,len(data))
    assert call('draw_list',dl)==1
    return int.from_bytes(C.string_at(ram+target,2),'big')

C.c_uint32.from_address(symbols['hd_height']).value=1440
for tile in (0,7):
    for alpha in (0,96,255):
        for weight in (0,128,255):
            coverage=255-((255-alpha)*weight+127)//255
            expected=((coverage>>3)<<11)|(((255-coverage)>>3)<<1)|1
            for triangle in (False,True):
                actual=sample(tile,alpha,weight,triangle)
                assert actual==expected,(tile,alpha,weight,triangle,hex(actual),hex(expected))
                if triangle:
                    offset=(value('triangle_id')&65535)*192+96
                    assert C.c_uint32.from_address(symbols['hd_triangles']+offset).value==0
print('36 rectangle/triangle alpha cases passed, including tile 7 wrapping to tile 0.')
