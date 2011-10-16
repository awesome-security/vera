#include <pro.h>
#include "arm_debmod.h"

//--------------------------------------------------------------------------
arm_debmod_t::arm_debmod_t()
{
  static const uchar bpt[] = ARM_BPT_CODE;
  bpt_code.append(bpt, sizeof(bpt));
  sp_idx = 13;
  pc_idx = 15;
  nregs = qnumber(arm_registers);

  is_xscale = false;
  databpts[0] = databpts[1] = BADADDR;
  codebpts[0] = codebpts[1] = BADADDR;
  dbcon = 0;
}


//--------------------------------------------------------------------------
int idaapi arm_debmod_t::dbg_is_ok_bpt(bpttype_t type, ea_t /*ea*/, int len)
{
  if ( type == BPT_SOFT )
    return BPT_OK;
  // for some reason hardware instructon breakpoints do not work
  if ( type == BPT_EXEC )
    return BPT_BAD_TYPE;
  if ( len > 4 )
    return BPT_BAD_LEN;
  bool ok = type == BPT_EXEC
    ? (codebpts[0] == BADADDR || codebpts[1] == BADADDR)
    : (databpts[0] == BADADDR || databpts[1] == BADADDR);
  return ok ? BPT_OK : BPT_TOO_MANY;
}

//--------------------------------------------------------------------------
bool arm_debmod_t::add_hwbpt(bpttype_t type, ea_t ea, int len)
{
  //  msg("add_hwbpt %d %a %d\n", type, ea, len);
  if ( !is_xscale || len > 4 )
    return false;

  if ( !init_hwbpt_support() )
    return false;

  if ( type == BPT_EXEC )
  {
    if ( codebpts[0] != BADADDR && codebpts[1] != BADADDR )
      return false;
    int slot = codebpts[0] != BADADDR;
    codebpts[slot] = ea;
  }
  else
  {
    if ( databpts[0] != BADADDR && databpts[1] != BADADDR )
      return false;
    int slot = databpts[0] != BADADDR;
    int bits;
    switch ( type )
    {
    case BPT_WRITE:
      bits = 1;               // store only
      break;
    case BPT_RDWR:
      bits = 2;               // load/store
      break;
      //      BPT_READ:               // load only
      //        bits = 3;
      //        break;
    default:
      return false;
    }
    databpts[slot] = ea;
    dbcon |= bits << (slot*2);
  }
  return enable_hwbpts();
}

//--------------------------------------------------------------------------
bool arm_debmod_t::del_hwbpt(ea_t ea)
{
  //  msg("del_hwbpt %a\n", ea);
  if ( databpts[0] == ea )
  {
    databpts[0] = BADADDR;
    dbcon &= ~3;
  }
  else if ( databpts[1] == ea )
  {
    databpts[1] = BADADDR;
    dbcon &= ~(3<<2);
  }
  else if ( codebpts[0] == ea )
  {
    codebpts[0] = BADADDR;
  }
  else if ( codebpts[1] == ea )
  {
    codebpts[1] = BADADDR;
  }
  else
  {
    return false;
  }
  return enable_hwbpts();
}

//--------------------------------------------------------------------------
void arm_debmod_t::cleanup_hwbpts()
{
  databpts[0] = BADADDR;
  databpts[1] = BADADDR;
  codebpts[0] = BADADDR;
  codebpts[1] = BADADDR;
  dbcon = 0;
  // disable all bpts
  if ( is_xscale )
    disable_hwbpts();
}

//--------------------------------------------------------------------------
int arm_debmod_t::finalize_appcall_stack(call_context_t &ctx, regval_map_t &regs, bytevec_t &/*stk*/)
{
  regs[14].ival = ctx.ctrl_ea;
  // return addrsize as the adjustment factor to add to sp
  // we do not need the return address, that's why it ignore the first 4
  // bytes of the prepared stack image
  return addrsize;
}

