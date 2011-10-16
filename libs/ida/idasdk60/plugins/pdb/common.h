#ifndef __PDB_COMMON_H__
#define __PDB_COMMON_H__

#define PDB_NODE_NAME             "$ pdb"
#define PDB_DLLBASE_NODE_IDX       0
#define PDB_DLLNAME_NODE_IDX       0

enum PDB_CALLCODE
{
  // user invoked 'load pdb' command, load pdb for the input file.
  // after invocation, result (boolean) is stored in: netnode(PDB_NODE_NAME).altval(PDB_DLLBASE_NODE_IDX)
  PDB_CC_USER = 0,
  // ida decided to call the plugin itself
  PDB_CC_IDA  = 1,
  // load pdb for an additional exe/dll
  //   load_addr: netnode(PDB_NODE_NAME).altval(PDB_DLLBASE_NODE_IDX)
  //   dll_name:  netnode(PDB_NODE_NAME).supstr(PDB_DLLNAME_NODE_IDX)
  PDB_CC_ADDITIONAL = 2
};

#endif
