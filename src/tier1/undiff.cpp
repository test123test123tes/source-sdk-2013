//========= Copyright Valve Corporation, All rights reserved. ============//
//
// UnDiff - Apply difference block
//
//=============================================================================//

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/diff.h"
#include "mathlib/mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void ApplyDiffs(uint8 const *OldBlock, uint8 const *DiffList,
                int OldSize, int DiffListSize, int &ResultListSize,uint8 *Output,uint32 OutSize)
{
  ResultListSize = 0;
  if ( !OldBlock || !DiffList || !Output || OldSize < 0 || DiffListSize < 0 )
    return;

  uint8 const *copy_src=OldBlock;
  uint8 const *old_end=OldBlock+OldSize;
  uint8 const *end_of_diff_list=DiffList+DiffListSize;
  uint8 const *obuf=Output;
  uint8 *out_end=Output+OutSize;
  while(DiffList<end_of_diff_list)
  {
    //  printf("dptr=%x ",DiffList-d);
    uint8 op=*(DiffList++);
    if (op==0)
    {
      if ( end_of_diff_list - DiffList < 4 )
        break;
      uint16 copy_sz=DiffList[0]+256*DiffList[1];
      int copy_ofs=DiffList[2]+DiffList[3]*256;
      if (copy_ofs>32767)
        copy_ofs|=0xffff0000;
    //    printf("long cp from %x to %x len=%d\n", copy_src+copy_ofs-OldBlock,Output-obuf,copy_sz);
      
      ptrdiff_t src_pos = (copy_src - OldBlock) + (ptrdiff_t)copy_ofs;
      if ( src_pos < 0 || src_pos > OldSize || (size_t)copy_sz > (size_t)(OldSize - src_pos) || (size_t)copy_sz > (size_t)(out_end - Output) )
        break;
      memcpy(Output,OldBlock+src_pos,copy_sz);
      Output+=copy_sz;
      copy_src=OldBlock+src_pos+copy_sz;
      DiffList+=4;
    }
    else
    {
      if (op & 0x80)
      {
        int copy_sz=op & 0x7f;
        int copy_ofs;
        if (copy_sz==0)
        {
          if ( end_of_diff_list - DiffList < 1 )
            break;
          copy_sz=DiffList[0];
          if (copy_sz==0)
          {
            // big raw copy
            if ( end_of_diff_list - DiffList < 4 )
              break;
            copy_sz=DiffList[1]+256*DiffList[2]+65536*DiffList[3];
            if ( copy_sz < 0 || (size_t)copy_sz > (size_t)(end_of_diff_list - (DiffList + 4)) || (size_t)copy_sz > (size_t)(out_end - Output) )
              break;
            memcpy(Output,DiffList+4,copy_sz);
    //          printf("big rawcopy to %x len=%d\n", Output-obuf,copy_sz);
            
            DiffList+=copy_sz+4;
            Output+=copy_sz;
          }
          else
          {
            if ( end_of_diff_list - DiffList < 3 )
              break;
            copy_ofs=DiffList[1]+(DiffList[2]*256);
            if (copy_ofs>32767)
              copy_ofs|=0xffff0000;
    //          printf("long ofs cp from %x to %x len=%d\n", copy_src+copy_ofs-OldBlock,Output-obuf,copy_sz);
            
            ptrdiff_t src_pos = (copy_src - OldBlock) + (ptrdiff_t)copy_ofs;
            if ( src_pos < 0 || src_pos > OldSize || (size_t)copy_sz > (size_t)(OldSize - src_pos) || (size_t)copy_sz > (size_t)(out_end - Output) )
              break;
            memcpy(Output,OldBlock+src_pos,copy_sz);
            Output+=copy_sz;
            copy_src=OldBlock+src_pos+copy_sz;
            DiffList+=3;
          }
        }
        else
        {
          if ( end_of_diff_list - DiffList < 1 )
            break;
          copy_ofs=DiffList[0];
          if (copy_ofs>127)
            copy_ofs|=0xffffff80;
    //        printf("cp from %x to %x len=%d\n", copy_src+copy_ofs-OldBlock,Output-obuf,copy_sz);
          
          ptrdiff_t src_pos = (copy_src - OldBlock) + (ptrdiff_t)copy_ofs;
          if ( src_pos < 0 || src_pos > OldSize || (size_t)copy_sz > (size_t)(OldSize - src_pos) || (size_t)copy_sz > (size_t)(out_end - Output) )
            break;
          memcpy(Output,OldBlock+src_pos,copy_sz);
          Output+=copy_sz;
          copy_src=OldBlock+src_pos+copy_sz;
          DiffList++;
        }
      }
      else
      {
    //      printf("raw copy %d to %x\n",op & 127,Output-obuf);
        int raw_sz = op & 127;
        if ( (size_t)raw_sz > (size_t)(end_of_diff_list - DiffList) || (size_t)raw_sz > (size_t)(out_end - Output) )
          break;
        memcpy(Output,DiffList,raw_sz);
        Output+=raw_sz;
        DiffList+=raw_sz;
      }
    }
  }
  (void)old_end;
  ResultListSize=Output-obuf;
  
}
