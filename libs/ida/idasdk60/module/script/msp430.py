# ----------------------------------------------------------------------
# Texas Instruments MSP430 processor module
# Copyright (c) 2010 Hex-Rays
#
# This module demonstrates:
#  - instruction decoding and printing
#  - simplification of decoded instructions
#  - creation of code and data cross-references
#  - auto-creation of data items from cross-references
#  - tracing of the stack pointer changes
#  - creation of the stack variables
#  - handling of switch constructs
#
# Please send fixes or improvements to support@hex-rays.com

import sys
import idaapi
from idaapi import *

# extract bitfield occupying bits high..low from val (inclusive, start from 0)
def BITS(val, high, low):
  return (val>>low)&((1<<(high-low+1))-1)

# extract one bit
def BIT(val, bit):
  return (val>>bit) & 1

# sign extend b low bits in x
# from "Bit Twiddling Hacks"
def SIGNEXT(x, b):
  m = 1 << (b - 1)
  x = x & ((1 << b) - 1)
  return (x ^ m) - m

# values for specval field for o_phrase operands
FL_INDIRECT = 1  # indirect: @Rn
FL_AUTOINC  = 2  # auto-increment: @Rn+

# values for specval field for o_mem operands
FL_ABSOLUTE = 1  # absolute: &addr
FL_SYMBOLIC = 2  # symbolic: addr

# values for cmd.auxpref
AUX_NOSUF   = 0  # no suffix (e.g. SWPB)
AUX_WORD    = 1  # word transfer, .W suffix
AUX_BYTE    = 2  # byte transfer, .B suffix
AUX_A       = 3  # 20-bit transfer, .A suffix

# check if operand is register reg
def is_reg(op, reg):
    return op.type == o_reg and op.reg == reg

# check if operand is immediate value val
def is_imm(op, val):
    return op.type == o_imm and op.value == val

# are operands equal?
def same_op(op1, op2):
    return op1.type  == op2.type  and \
           op1.reg   == op2.reg   and \
           op1.value == op2.value and \
           op1.addr  == op2.addr  and \
           op1.flags == op2.flags and \
           op1.specval == op2.specval and \
           op1.dtyp == op2.dtyp

# is operand auto-increment register reg?
def is_autoinc(op, reg):
    return op.type == o_phrase and op.reg == reg and op.specval == FL_AUTOINC

# is sp delta fixed by the user?
def is_fixed_spd(ea):
    return (get_aflags(ea) & AFL_FIXEDSPD) != 0

# ----------------------------------------------------------------------
class msp430_processor_t(idaapi.processor_t):
    """
    Processor module classes must derive from idaapi.processor_t
    """

    # IDP id ( Numbers above 0x8000 are reserved for the third-party modules)
    id = idaapi.PLFM_MSP430

    # Processor features
    flag = PR_SEGS | PRN_HEX | PR_RNAMESOK | PR_NO_SEGMOVE | PR_WORD_INS

    # Number of bits in a byte for code segments (usually 8)
    # IDA supports values up to 32 bits
    cnbits = 8

    # Number of bits in a byte for non-code segments (usually 8)
    # IDA supports values up to 32 bits
    dnbits = 8

    # short processor names
    # Each name should be shorter than 9 characters
    psnames = ['msp430']

    # long processor names
    # No restriction on name lengthes.
    plnames = ['Texas Instruments MSP430']

    # size of a segment register in bytes
    segreg_size = 0

    # Array of typical code start sequences (optional)
    codestart = ['\x0B\x12']  # 120B: push R11

    # Array of 'return' instruction opcodes (optional)
    # retcodes = ['\x30\x41']   # 4130: ret (mov.w @SP+, PC)

    # Array of instructions
    instruc = [
        {'name': '',  'feature': 0},                                # placeholder for "not an instruction"

        # two-operand instructions
        {'name': 'mov',  'feature': CF_USE1 | CF_CHG2,             'cmt': "Move source to destination"},
        {'name': 'add',  'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Add source to destination"},
        {'name': 'addc', 'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Add source and carry to destination"},
        {'name': 'sub',  'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Subtract source from destination"},
        {'name': 'subc', 'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Subtract source with carry from destination"},
        {'name': 'cmp',  'feature': CF_USE1 | CF_USE2,             'cmt': "Compare source and destination"},
        {'name': 'dadd', 'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Add source decimally to destination"},
        {'name': 'bit',  'feature': CF_USE1 | CF_USE2,             'cmt': "Test bits set in source in destination"},
        {'name': 'bic',  'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Clear bits set in source in destination"},
        {'name': 'bis',  'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Set bits set in source in destination"},
        {'name': 'xor',  'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Exclusive OR source with destination"},
        {'name': 'and',  'feature': CF_USE1 | CF_USE2 | CF_CHG2,   'cmt': "Binary AND source and destination"},

        # one-operand instructions
        {'name': 'rrc',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Rotate right through C"},
        {'name': 'swpb', 'feature': CF_USE1 | CF_CHG1,   'cmt': "Swap bytes"},
        {'name': 'rra',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Rotate right arithmetically"},
        {'name': 'sxt',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Extend sign (8 bits to 16)"},
        {'name': 'push', 'feature': CF_USE1          ,   'cmt': "Push onto stack"},
        {'name': 'call', 'feature': CF_USE1          ,   'cmt': "Call subroutine"},
        {'name': 'reti', 'feature': CF_STOP          ,   'cmt': "Return from interrupt"},

        # jumps
        {'name': 'jnz',  'feature': CF_USE1          ,   'cmt': "Jump if not zero/not equal"},
        {'name': 'jz',   'feature': CF_USE1          ,   'cmt': "Jump if zero/equal"},
        {'name': 'jc',   'feature': CF_USE1          ,   'cmt': "Jump if carry/higher or same (unsigned)"},
        {'name': 'jnc',  'feature': CF_USE1          ,   'cmt': "Jump if no carry/lower (unsigned)"},
        {'name': 'jn',   'feature': CF_USE1          ,   'cmt': "Jump if negative"},
        {'name': 'jge',  'feature': CF_USE1          ,   'cmt': "Jump if greater or equal (signed)"},
        {'name': 'jl',   'feature': CF_USE1          ,   'cmt': "Jump if less (signed)"},
        {'name': 'jmp',  'feature': CF_USE1 | CF_STOP,   'cmt': "Jump unconditionally"},

        # emulated instructions
        {'name': 'adc',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Add carry to destination"},
        {'name': 'br',   'feature': CF_USE1 | CF_STOP,   'cmt': "Branch to destination"},
        {'name': 'clr',  'feature': CF_CHG1          ,   'cmt': "Clear destination"},
        {'name': 'clrc', 'feature': 0                ,   'cmt': "Clear carry bit"},
        {'name': 'clrn', 'feature': 0                ,   'cmt': "Clear negative bit"},
        {'name': 'clrz', 'feature': 0                ,   'cmt': "Clear zero bit"},
        {'name': 'dadc', 'feature': CF_USE1 | CF_CHG1,   'cmt': "Add carry decimally to destination"},
        {'name': 'dec',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Decrement destination"},
        {'name': 'decd', 'feature': CF_USE1 | CF_CHG1,   'cmt': "Double-decrement destination"},
        {'name': 'dint', 'feature': 0                ,   'cmt': "Disable general interrupts"},
        {'name': 'eint', 'feature': 0                ,   'cmt': "Enable general interrupts"},
        {'name': 'inc',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Increment destination"},
        {'name': 'incd', 'feature': CF_USE1 | CF_CHG1,   'cmt': "Double-increment destination"},
        {'name': 'inv',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Invert destination"},
        {'name': 'nop',  'feature': 0                ,   'cmt': "No operation"},
        {'name': 'pop',  'feature': CF_CHG1          ,   'cmt': "Pop from the stack"},
        {'name': 'ret',  'feature': CF_STOP          ,   'cmt': "Return from subroutine"},
        {'name': 'rla',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Rotate left arithmetically"},
        {'name': 'rlc',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Rotate left through carry"},
        {'name': 'sbc',  'feature': CF_USE1 | CF_CHG1,   'cmt': "Substract borrow (=NOT carry) from destination"},
        {'name': 'setc', 'feature': 0                ,   'cmt': "Set carry bit"},
        {'name': 'setn', 'feature': 0                ,   'cmt': "Set negative bit"},
        {'name': 'setz', 'feature': 0                ,   'cmt': "Set zero bit"},
        {'name': 'tst',  'feature': CF_USE1          ,   'cmt': "Test destination"},
    ]

    # icode of the first instruction
    instruc_start = 0

    # icode of the last instruction + 1
    instruc_end = len(instruc) + 1

    # Size of long double (tbyte) for this processor (meaningful only if ash.a_tbyte != NULL) (optional)
    # tbyte_size = 0

    #
    # Number of digits in floating numbers after the decimal point.
    # If an element of this array equals 0, then the corresponding
    # floating point data is not used for the processor.
    # This array is used to align numbers in the output.
    #      real_width[0] - number of digits for short floats (only PDP-11 has them)
    #      real_width[1] - number of digits for "float"
    #      real_width[2] - number of digits for "double"
    #      real_width[3] - number of digits for "long double"
    # Example: IBM PC module has { 0,7,15,19 }
    #
    # (optional)
    real_width = (0, 7, 0, 0)


    # only one assembler is supported
    assembler = {
        # flag
        'flag' : ASH_HEXF0 | ASD_DECF0 | ASO_OCTF5 | ASB_BINF0 | AS_N2CHR,

        # user defined flags (local only for IDP) (optional)
        'uflag' : 0,

        # Assembler name (displayed in menus)
        'name': "Generic MSP430 assembler",

        # array of automatically generated header lines they appear at the start of disassembled text (optional)
        'header': [".msp430"],

        # array of unsupported instructions (array of cmd.itype) (optional)
        #'badworks': [],

        # org directive
        'origin': ".org",

        # end directive
        'end': ".end",

        # comment string (see also cmnt2)
        'cmnt': ";",

        # ASCII string delimiter
        'ascsep': "\"",

        # ASCII char constant delimiter
        'accsep': "'",

        # ASCII special chars (they can't appear in character and ascii constants)
        'esccodes': "\"'",

        #
        #      Data representation (db,dw,...):
        #
        # ASCII string directive
        'a_ascii': ".char",

        # byte directive
        'a_byte': ".byte",

        # word directive
        'a_word': ".short",

        # remove if not allowed
        'a_dword': ".long",

        # remove if not allowed
        # 'a_qword': "dq",

        # float;  4bytes; remove if not allowed
        'a_float': ".float",

        # uninitialized data directive (should include '%s' for the size of data)
        'a_bss': ".space %s",

        # 'equ' Used if AS_UNEQU is set (optional)
        'a_equ': ".equ",

        # 'seg ' prefix (example: push seg seg001)
        'a_seg': "seg",

        # current IP (instruction pointer) symbol in assembler
        'a_curip': "$",

        # "public" name keyword. NULL-gen default, ""-do not generate
        'a_public': ".def",

        # "weak"   name keyword. NULL-gen default, ""-do not generate
        'a_weak': "",

        # "extrn"  name keyword
        'a_extrn': ".ref",

        # "comm" (communal variable)
        'a_comdef': "",

        # "align" keyword
        'a_align': ".align",

        # Left and right braces used in complex expressions
        'lbrace': "(",
        'rbrace': ")",

        # %  mod     assembler time operation
        'a_mod': "%",

        # &  bit and assembler time operation
        'a_band': "&",

        # |  bit or  assembler time operation
        'a_bor': "|",

        # ^  bit xor assembler time operation
        'a_xor': "^",

        # ~  bit not assembler time operation
        'a_bnot': "~",

        # << shift left assembler time operation
        'a_shl': "<<",

        # >> shift right assembler time operation
        'a_shr': ">>",

        # size of type (format string) (optional)
        'a_sizeof_fmt': "size %s",

        'flag2': 0,

        # the include directive (format string) (optional)
        'a_include_fmt': '.include "%s"',
    } # Assembler


    # ----------------------------------------------------------------------
    # The following callbacks are optional
    #

    #def notify_newprc(self, nproc):
    #    """
    #    Before changing proccesor type
    #    nproc - processor number in the array of processor names
    #    return 1-ok,0-prohibit
    #    """
    #    return 1

    #def notify_assemble(self, ea, cs, ip, use32, line):
    #    """
    #    Assemble an instruction
    #     (make sure that PR_ASSEMBLE flag is set in the processor flags)
    #     (display a warning if an error occurs)
    #     args:
    #       ea -  linear address of instruction
    #       cs -  cs of instruction
    #       ip -  ip of instruction
    #       use32 - is 32bit segment?
    #       line - line to assemble
    #    returns the opcode string
    #    """
    #    pass

    def get_frame_retsize(self, func_ea):
        """
        Get size of function return address in bytes
        If this function is absent, the kernel will assume
             4 bytes for 32-bit function
             2 bytes otherwise
        """
        return 2

    def notify_get_autocmt(self):
        """
        Get instruction comment. 'cmd' describes the instruction in question
        @return: None or the comment string
        """
        if 'cmt' in self.instruc[self.cmd.itype]:
          return self.instruc[self.cmd.itype]['cmt']

    # ----------------------------------------------------------------------
    def is_movpc(self):
        # mov xxx, PC
        return self.cmd.itype == self.itype_mov and is_reg(self.cmd.Op2, self.ireg_PC) and self.cmd.auxpref == AUX_WORD

    # ----------------------------------------------------------------------
    def handle_operand(self, op, isRead):
        uFlag     = self.get_uFlag()
        is_offs   = isOff(uFlag, op.n)
        dref_flag = dr_R if isRead else dr_W
        def_arg   = isDefArg(uFlag, op.n)
        optype    = op.type

        itype = self.cmd.itype
        # create code xrefs
        if optype == o_imm:
            makeoff = False
            if itype == self.itype_call:
                # call #func
                ua_add_cref(op.offb, op.value, fl_CN)
                makeoff = True
            elif self.is_movpc() or self.cmd.itype == self.itype_br:
                # mov #addr, PC
                ua_add_cref(op.offb, op.value, fl_JN)
                makeoff = True
            if makeoff and not def_arg:
                set_offset(self.cmd.ea, op.n, self.cmd.cs)
                is_offs = True
            if is_offs:
                ua_add_off_drefs(op, dr_O)
        # create data xrefs
        elif optype == o_displ:
            # delta(reg)
            if is_offs:
                ua_add_off_drefs(op, dref_flag)
            elif may_create_stkvars() and not def_arg and op.reg == self.ireg_SP:
                # var_x(SP)
                pfn = get_func(self.cmd.ea)
                if pfn and ua_stkvar2(op, op.addr, STKVAR_VALID_SIZE):
                    op_stkvar(self.cmd.ea, op.n)
        elif optype == o_mem:
            if op.specval == FL_SYMBOLIC:
                dref_flag = dr_O
            else:
                ua_dodata2(op.offb, op.addr, op.dtyp)
            ua_add_dref(op.offb, op.addr, dref_flag)
        elif optype == o_near:
            ua_add_cref(op.offb, op.addr, fl_JN)

    # ----------------------------------------------------------------------
    def add_stkpnt(self, pfn, v):
        if pfn:
            end = self.cmd.ea + self.cmd.size
            if not is_fixed_spd(end):
                add_auto_stkpnt2(pfn, end, v)

    # ----------------------------------------------------------------------
    def trace_sp(self):
        """
        Trace the value of the SP and create an SP change point if the current
        instruction modifies the SP.
        """
        pfn = get_func(self.cmd.ea)
        if not pfn:
            return
        if self.cmd.itype in [self.itype_add, self.itype_addc, self.itype_sub, self.itype_subc] and \
           is_reg(cmd.Op2, self.ireg_SP) and self.cmd.auxpref == AUX_WORD and \
           self.cmd.Op1.type == o_imm:
            # add.w  #xxx, SP
            # subc.w #xxx, SP
            spofs = SIGNEXT(self.cmd.Op1.value, 16)
            if self.cmd.itype in [self.itype_sub, self.itype_subc]:
                spofs = -spofs
            self.add_stkpnt(pfn, spofs)
        elif self.cmd.itype == self.itype_push:
            self.add_stkpnt(pfn, -2)
        elif self.cmd.itype == self.itype_pop or is_autoinc(self.cmd.Op1, self.ireg_SP):
            # pop R7 or mov.w @SP+, R7
            self.add_stkpnt(pfn, +2)

    # ----------------------------------------------------------------------
    def check_switch(self):
        # detect switches and set switch info
        #
        #       cmp.w   #nn, Rx
        #       jnc     default
        #       [mov.w   Rx, Ry]
        #       rla.w   Ry, Ry
        #       br      jtbl(Ry)
        # jtbl  .short case0, .short case1
        if get_switch_info_ex(self.cmd.ea):
            return
        ok = False
        si = switch_info_ex_t()
        saved_cmd = self.cmd.copy()

        # mov.w   jtbl(Ry), PC
        if (self.is_movpc() or self.cmd.itype == self.itype_br) and self.cmd.Op1.type == o_displ:
            si.jumps = self.cmd.Op1.addr # jump table address
            Ry = self.cmd.Op1.reg
            ok = True
            # add.w   Ry, Ry  | rla.w Ry
            if decode_prev_insn(self.cmd.ea) != BADADDR and self.cmd.auxpref == AUX_WORD:
               ok = self.cmd.itype == self.itype_add and is_reg(self.cmd.Op1, Ry) and is_reg(self.cmd.Op2, Ry) or \
                    self.cmd.itype == self.itype_rla and is_reg(self.cmd.Op1, Ry)
            else:
                ok = False
            if ok and decode_prev_insn(self.cmd.ea) != BADADDR:
               # mov.w   Rx, Ry
               if self.cmd.itype == self.itype_mov and \
                  is_reg(self.cmd.Op2, Ry) and \
                  cmd.Op1.type == o_reg and \
                  self.cmd.auxpref == AUX_WORD:
                   Rx = cmd.Op1.reg
                   ok = decode_prev_insn(self.cmd.ea) != BADADDR
               else:
                   Rx = Ry
            else:
                ok = False

            # jnc default
            if ok and self.cmd.itype == self.itype_jnc:
                si.defjump = self.cmd.Op1.addr
            else:
                ok = False

            # cmp.w   #nn, Rx
            if ok and decode_prev_insn(self.cmd.ea) == BADADDR or \
               self.cmd.itype != self.itype_cmp or \
               self.cmd.Op1.type != o_imm or \
               not is_reg(self.cmd.Op2, Rx) or \
               self.cmd.auxpref != AUX_WORD:
                ok = False
            else:
                si.flags |= SWI_DEFAULT
                si.ncases = cmd.Op1.value
                si.lowcase = 0
                si.startea = self.cmd.ea
                si.set_expr(Rx, dt_word)

        self.cmd.assign(saved_cmd)
        if ok:
            # make offset to the jump table
            set_offset(self.cmd.ea, 0, self.cmd.cs)
            set_switch_info_ex(self.cmd.ea, si)
            create_switch_table(self.cmd.ea, si)

    # ----------------------------------------------------------------------
    # The following callbacks are mandatory
    #
    def emu(self):
        """
        Emulate instruction, create cross-references, plan to analyze
        subsequent instructions, modify flags etc. Upon entrance to this function
        all information about the instruction is in 'cmd' structure.
        If zero is returned, the kernel will delete the instruction.
        """
        aux = self.get_auxpref()
        Feature = self.cmd.get_canon_feature()

        if Feature & CF_USE1:
            self.handle_operand(self.cmd.Op1, 1)
        if Feature & CF_CHG1:
            self.handle_operand(self.cmd.Op1, 0)
        if Feature & CF_USE2:
            self.handle_operand(self.cmd.Op2, 1)
        if Feature & CF_CHG2:
            self.handle_operand(self.cmd.Op2, 0)
        if Feature & CF_JUMP:
            QueueMark(Q_jumps, self.cmd.ea)

        # is it an unconditional jump?
        uncond_jmp = self.cmd.itype == self.itype_jmp or self.is_movpc() or self.cmd.itype == self.itype_br

        # add flow
        flow = (Feature & CF_STOP == 0) and not uncond_jmp
        if flow:
            ua_add_cref(0, self.cmd.ea + self.cmd.size, fl_F)
        else:
            self.check_switch()

        # trace the stack pointer if:
        #   - it is the second analysis pass
        #   - the stack pointer tracing is allowed
        if may_trace_sp():
            if flow:
                self.trace_sp()         # trace modification of SP register
            else:
                recalc_spd(self.cmd.ea) # recalculate SP register for the next insn

        return 1

    # ----------------------------------------------------------------------
    def outop(self, op):
        """
        Generate text representation of an instructon operand.
        This function shouldn't change the database, flags or anything else.
        All these actions should be performed only by the emu() function.
        The output text is placed in the output buffer initialized with init_output_buffer()
        This function uses out_...() functions from ua.hpp to generate the operand text
        Returns: 1-ok, 0-operand is hidden.
        """
        optype = op.type
        fl     = op.specval
        signed = 0

        if optype == o_reg:
            out_register(self.regNames[op.reg])
        elif optype == o_imm:
            out_symbol('#')
            width = OOFW_16
            if self.cmd.auxpref == AUX_BYTE:
                width = OOFW_8
            OutValue(op, OOFW_IMM | signed | width )
        elif optype in [o_near, o_mem]:
            if optype == o_mem and fl == FL_ABSOLUTE:
                out_symbol('&')
            r = out_name_expr(op, op.addr, BADADDR)
            if not r:
                out_tagon(COLOR_ERROR)
                OutLong(op.addr, 16)
                out_tagoff(COLOR_ERROR)
                QueueMark(Q_noName, self.cmd.ea)
        elif optype == o_displ:
            OutValue(op, OOF_ADDR | OOFW_16 | signed )
            out_symbol('(')
            out_register(self.regNames[op.reg])
            out_symbol(')')
        elif optype == o_phrase:
            out_symbol('@')
            out_register(self.regNames[op.reg])
            if fl == FL_AUTOINC:
              out_symbol('+')
        else:
            return False

        return True

    # ----------------------------------------------------------------------
    def out(self):
        """
        Generate text representation of an instruction in 'cmd' structure.
        This function shouldn't change the database, flags or anything else.
        All these actions should be performed only by emu() function.
        Returns: nothing
        """
        # Init output buffer
        buf = idaapi.init_output_buffer(1024)

        postfix = ""

        # add postfix if necessary
        if self.cmd.auxpref == AUX_BYTE:
            postfix = ".b"
        elif self.cmd.auxpref == AUX_WORD:
            postfix = ".w"

        # first argument (12) is the width of the mnemonic field
        OutMnem(12, postfix)

        # output first operand
        # kernel will call outop()
        if self.cmd.Op1.type != o_void:
            out_one_operand(0)

        # output the rest of operands separated by commas
        for i in xrange(1, 3):
            if self.cmd[i].type == o_void:
                break
            out_symbol(',')
            OutChar(' ')
            out_one_operand(i)

        term_output_buffer()
        cvar.gl_comm = 1 # generate comment at the next call to MakeLine()
        MakeLine(buf)

    # ----------------------------------------------------------------------
    # fill operand fields from decoded instruction parts
    # op:  operand to be filled in
    # reg: register number
    # A:   adressing mode
    # BW:  value of the B/W (byte/word) field
    # is_source: true if filling source operand (used for decoding constant generators)
    def fill_op(self, op, reg, A, BW, is_source = False):
        op.reg  = reg
        op.dtyp = dt_word if BW == 0 else dt_byte
        if is_source:
            # check for constant generators
            if reg == self.ireg_SR and A >= 2:
                op.type = o_imm
                op.value = [4, 8] [A-2]
                return
            elif reg == self.ireg_R3:
                op.type = o_imm
                op.value = [0, 1, 2, 0xFFFF] [A]
                return
        if A == 0:
            # register mode
            op.type = o_reg
            op.dtyp = dt_word
        elif A == 1:
            if reg == self.ireg_SR:
                # absolute address mode
                op.type = o_mem
                op.specval = FL_ABSOLUTE
                op.offb = self.cmd.size
                op.addr = ua_next_word()
            else:
                # indexed mode
                # map it to IDA's displacement
                op.type = o_displ
                op.offb = self.cmd.size
                op.addr = ua_next_word()
        elif A == 2:
            # Indirect register mode
            # map it to o_phrase
            op.type = o_phrase
            op.specval = FL_INDIRECT
        elif A == 3:
            # Indirect autoincrement
            # map it to o_phrase
            if reg == self.ireg_PC:
                #this is actually immediate mode
                op.type = o_imm
                op.offb = self.cmd.size
                op.value = ua_next_word()
            else:
                op.type = o_phrase
                op.specval = FL_AUTOINC

    # ----------------------------------------------------------------------
    def decode_format_I(self, w):
        #  Double-Operand (Format I) Instructions
        #
        #   15 ... 12 11 ... 8   7    6   5  4 3    0
        #  +---------+--------+----+-----+----+------+
        #  | Op-code |  Rsrc  | Ad | B/W | As | Rdst |
        #  +---------+--------+----+-----+----+------+
        #  |       Source or destination 15:0        |
        #  +-----------------------------------------+
        #  |             Destination 15:0            |
        #  +-----------------------------------------+
        opc  = BITS(w, 15, 12)
        As   = BITS(w, 5, 4)
        Ad   = BIT(w, 7)
        Rsrc = BITS(w, 11, 8)
        Rdst = BITS(w,  3, 0)
        BW   = BIT(w, 6)
        self.fill_op(self.cmd.Op1, Rsrc, As, BW, True)
        self.fill_op(self.cmd.Op2, Rdst, Ad, BW)
        self.cmd.auxpref = BW + AUX_WORD
        if opc < 4:
          # something went wrong
          self.cmd.size = 0
        else:
          self.cmd.itype = self.itype_mov + (opc-4)

    # ----------------------------------------------------------------------
    def decode_format_II(self, w):
        #  Single-Operand (Format II) Instructions
        #
        #   15         10 9       7   6   5  4 3    0
        #  +-------------+---------+-----+----+------+
        #  | 0 0 0 1 0 0 | Op-code | B/W | Ad | Rdst |
        #  +-------------+---------+-----+----+------+
        #  |             Destination 15:0            |
        #  +-----------------------------------------+
        opc  = BITS(w, 9, 7)
        Ad   = BITS(w, 5, 4)
        Rdst = BITS(w, 3, 0)
        BW   = BIT(w, 6)
        if opc == 7:
            return
        self.fill_op(self.cmd.Op1, Rdst, Ad, BW)
        self.cmd.auxpref = BW + AUX_WORD
        self.cmd.itype = self.itype_rrc + opc
        if self.cmd.itype in [self.itype_swpb, self.itype_sxt, self.itype_call, self.itype_reti]:
            # these commands have no suffix and should have BW set to 0
            if BW == 0:
                self.cmd.auxpref = AUX_NOSUF
                if self.cmd.itype == self.itype_reti:
                    # Ad and Rdst should be 0
                    if Ad == 0 and Rdst == 0:
                        self.cmd.Op1.type = o_void
                    else:
                        # bad instruction
                        self.cmd.itype = self.itype_null
            else:
                # bad instruction
                self.cmd.itype = self.itype_null

    # ----------------------------------------------------------------------
    def decode_jump(self, w):
        #  Jump Instructions
        #
        #   15     13 12   10 9                    0
        #  +---------+-------+----------------------+
        #  |  0 0 1  |   C   |  10-bit PC offset    |
        #  +---------+-------+----------------------+
        C    = BITS(w, 12, 10)
        offs = BITS(w,  9,  0)
        offs = SIGNEXT(offs, 10)
        self.cmd.Op1.type = o_near
        self.cmd.Op1.addr = self.cmd.ea + 2 + offs*2
        self.cmd.itype = self.itype_jnz + C

    # ----------------------------------------------------------------------
    # does operand match tuple m? (type, value)
    def match_op(self, op, m):
        if m == None:
            return True
        if op.type != m[0]:
            return False
        if op.type == o_imm:
            return op.value == m[1]
        elif op.type in [o_reg, o_phrase]:
            return op.reg == m[1]
        else:
            return false

    # ----------------------------------------------------------------------
    # replace some instructions by simplified mnemonics ("emulated" in TI terms)
    def simplify(self):
        # source mnemonic mapped to a list of matches:
        #   match function, new mnemonic, new operand
        maptbl = {
            self.itype_addc: [
                # addc #0, dst -> adc dst
                [ lambda: is_imm(self.cmd.Op1, 0), self.itype_adc, 2 ],
                # addc dst, dst -> rlc dst
                [ lambda: same_op(self.cmd.Op1, self.cmd.Op2), self.itype_rlc, 1 ],
            ],
            self.itype_mov: [
                # mov #0, R3 -> nop
                [ lambda: is_imm(self.cmd.Op1, 0) and is_reg(self.cmd.Op2, self.ireg_R3), self.itype_nop, 0 ],
                # mov #0, dst -> clr dst
                [ lambda: is_imm(self.cmd.Op1, 0), self.itype_clr, 2 ],
                # mov @SP+, PC -> ret
                [ lambda: is_autoinc(self.cmd.Op1, self.ireg_SP) and is_reg(self.cmd.Op2, self.ireg_PC), self.itype_ret, 0 ],
                # mov @SP+, dst -> pop dst
                [ lambda: is_autoinc(self.cmd.Op1, self.ireg_SP), self.itype_pop, 2 ],
                # mov dst, PC -> br dst
                [ lambda: self.is_movpc(), self.itype_br, 1 ],
            ],
            self.itype_bic: [
                # bic #1, SR -> clrc
                [ lambda: is_imm(self.cmd.Op1, 1) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_clrc, 0 ],
                # bic #2, SR -> clrz
                [ lambda: is_imm(self.cmd.Op1, 2) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_clrz, 0 ],
                # bic #4, SR -> clrn
                [ lambda: is_imm(self.cmd.Op1, 4) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_clrn, 0 ],
                # bic #8, SR -> dint
                [ lambda: is_imm(self.cmd.Op1, 8) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_dint, 0 ],
            ],
            self.itype_bis: [
                # bis #1, SR -> setc
                [ lambda: is_imm(self.cmd.Op1, 1) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_setc, 0 ],
                # bis #2, SR -> setz
                [ lambda: is_imm(self.cmd.Op1, 2) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_setz, 0 ],
                # bis #4, SR -> setn
                [ lambda: is_imm(self.cmd.Op1, 4) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_setn, 0 ],
                # bis #8, SR -> eint
                [ lambda: is_imm(self.cmd.Op1, 8) and is_reg(self.cmd.Op2, self.ireg_SR), self.itype_eint, 0 ],
            ],
            self.itype_dadd: [
                # dadd #0, dst -> dadc dst
                [ lambda: is_imm(self.cmd.Op1, 0), self.itype_dadc, 2 ],
            ],
            self.itype_sub: [
                # sub #1, dst -> dec dst
                [ lambda: is_imm(self.cmd.Op1, 1), self.itype_dec, 2 ],
                # sub #2, dst -> decd dst
                [ lambda: is_imm(self.cmd.Op1, 2), self.itype_decd, 2 ],
            ],
            self.itype_subc: [
                # subc #0, dst -> sbc dst
                [ lambda: is_imm(self.cmd.Op1, 0), self.itype_sbc, 2 ],
            ],
            self.itype_add: [
                # add #1, dst -> inc dst
                [ lambda: is_imm(self.cmd.Op1, 1), self.itype_inc, 2 ],
                # add #2, dst -> incd dst
                [ lambda: is_imm(self.cmd.Op1, 2), self.itype_incd, 2 ],
                # add dst, dst -> rla dst
                [ lambda: same_op(self.cmd.Op1, self.cmd.Op2), self.itype_rla, 1 ],
            ],
            self.itype_xor: [
                # xor #-1, dst -> inv dst
                [ lambda: is_imm(self.cmd.Op1, 0xFFFF), self.itype_inv, 2 ],
            ],
            self.itype_cmp: [
                # cmp #0, dst -> tst dst
                [ lambda: is_imm(self.cmd.Op1, 0), self.itype_tst, 2 ],
            ],
        }

        # instructions which should have no suffix
        nosuff = [self.itype_ret, self.itype_br, self.itype_clrc, self.itype_clrn, self.itype_clrz,
                  self.itype_dint, self.itype_eint, self.itype_clrc, self.itype_nop, self.itype_pop,
                  self.itype_setc, self.itype_setn, self.itype_setz, ]

        if self.cmd.itype in maptbl:
            for m in maptbl[self.cmd.itype]:
                if m[0]():
                    # matched instruction; replace the itype
                    self.cmd.itype = m[1]
                    if m[2] == 0:
                        # remove the operands
                        self.cmd.Op1.type = o_void
                    elif m[2] == 2:
                        # replace first operand with the second
                        self.cmd.Op1.assign(self.cmd.Op2)
                    # remove the second operand, if any
                    self.cmd.Op2.type = o_void
                    # remove suffix if necessary
                    if self.cmd.itype in nosuff:
                        self.cmd.auxpref = 0
                    break

    # ----------------------------------------------------------------------
    def ana(self):
        """
        Decodes an instruction into self.cmd.
        Returns: self.cmd.size (=the size of the decoded instruction) or zero
        """
        if (self.cmd.ea & 1) != 0:
            return 0
        w = ua_next_word()
        if BITS(w, 15, 10) == 4:   # 000100
            self.decode_format_II(w)
        elif BITS(w, 15, 13) == 1: # 001
            self.decode_jump(w)
        else:
            self.decode_format_I(w)

        self.simplify()
        # Return decoded instruction size or zero
        return self.cmd.size if self.cmd.itype != self.itype_null else 0

    # ----------------------------------------------------------------------
    def init_instructions(self):
        Instructions = []
        i = 0
        for x in self.instruc:
            if x['name'] != '':
                setattr(self, 'itype_' + x['name'], i)
            else:
                setattr(self, 'itype_null', i)
            i += 1

        # icode of the last instruction + 1
        self.instruc_end = len(self.instruc) + 1

        # Icode of return instruction. It is ok to give any of possible return
        # instructions
        self.icode_return = self.itype_reti

    # ----------------------------------------------------------------------
    def init_registers(self):
        """This function parses the register table and creates corresponding ireg_XXX constants"""

        # Registers definition
        self.regNames = [
            # General purpose registers
            "PC", # R0
            "SP", # R1
            "SR", # R2, CG1
            "R3", # CG2
            "R4",
            "R5",
            "R6",
            "R7",
            "R8",
            "R9",
            "R10",
            "R11",
            "R12",
            "R13",
            "R14",
            "R15",
            # Fake segment registers
            "CS",
            "DS"
        ]

        # Create the ireg_XXXX constants
        for i in xrange(len(self.regNames)):
            setattr(self, 'ireg_' + self.regNames[i], i)

        # Segment register information (use virtual CS and DS registers if your
        # processor doesn't have segment registers):
        self.regFirstSreg = self.ireg_CS
        self.regLastSreg  = self.ireg_DS

        # number of CS register
        self.regCodeSreg = self.ireg_CS

        # number of DS register
        self.regDataSreg = self.ireg_DS

    # ----------------------------------------------------------------------
    def __init__(self):
        idaapi.processor_t.__init__(self)
        self.init_instructions()
        self.init_registers()

# ----------------------------------------------------------------------
# Every processor module script must provide this function.
# It should return a new instance of a class derived from idaapi.processor_t
def PROCESSOR_ENTRY():
    return msp430_processor_t()
