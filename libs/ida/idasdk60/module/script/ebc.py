# ----------------------------------------------------------------------
# EFI bytecode processor module
# (c) Hex-Rays
# Please send fixes or improvements to support@hex-rays.com

import sys
import idaapi
from idaapi import *

# ----------------------------------------------------------------------
class ebc_processor_t(idaapi.processor_t):
    # IDP id ( Numbers above 0x8000 are reserved for the third-party modules)
    id = idaapi.PLFM_EBC

    # Processor features
    flag = PR_ASSEMBLE | PR_SEGS | PR_DEFSEG32 | PR_USE32 | PRN_HEX | PR_RNAMESOK | PR_NO_SEGMOVE

    # Number of bits in a byte for code segments (usually 8)
    # IDA supports values up to 32 bits
    cnbits = 8

    # Number of bits in a byte for non-code segments (usually 8)
    # IDA supports values up to 32 bits
    dnbits = 8

    # short processor names
    # Each name should be shorter than 9 characters
    psnames = ['ebc']

    # long processor names
    # No restriction on name lengthes.
    plnames = ['EFI Byte code']

    # size of a segment register in bytes
    segreg_size = 0

    # You should define 2 virtual segment registers for CS and DS.
    # Let's call them rVcs and rVds.

    # icode of the first instruction
    instruc_start = 0

    #
    #      Size of long double (tbyte) for this processor
    #      (meaningful only if ash.a_tbyte != NULL)
    #
    tbyte_size = 0

    # only one assembler is supported
    assembler = {
        # flag
        'flag' : ASH_HEXF3 | AS_UNEQU | AS_COLON | ASB_BINF4 | AS_N2CHR,

        # user defined flags (local only for IDP)
        # you may define and use your own bits
        'uflag' : 0,

        # Assembler name (displayed in menus)
        'name': "EFI bytecode assembler",

        # org directive
        'origin': "org",

        # end directive
        'end': "end",

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
        'a_ascii': "db",

        # byte directive
        'a_byte': "db",

        # word directive
        'a_word': "dw",

        # remove if not allowed
        'a_dword': "dd",

        # remove if not allowed
        'a_qword': "dq",

        # remove if not allowed
        'a_oword': "xmmword",

        # float;  4bytes; remove if not allowed
        'a_float': "dd",

        # double; 8bytes; NULL if not allowed
        'a_double': "dq",

        # long double;    NULL if not allowed
        'a_tbyte': "dt",

        # array keyword. the following
        # sequences may appear:
        #      #h - header
        #      #d - size
        #      #v - value
        #      #s(b,w,l,q,f,d,o) - size specifiers
        #                        for byte,word,
        #                            dword,qword,
        #                            float,double,oword
        'a_dups': "#d dup(#v)",

        # uninitialized data directive (should include '%s' for the size of data)
        'a_bss': "%s dup ?",

        # 'seg ' prefix (example: push seg seg001)
        'a_seg': "seg",

        # current IP (instruction pointer) symbol in assembler
        'a_curip': "$",

        # "public" name keyword. NULL-gen default, ""-do not generate
        'a_public': "public",

        # "weak"   name keyword. NULL-gen default, ""-do not generate
        'a_weak': "weak",

        # "extrn"  name keyword
        'a_extrn': "extrn",

        # "comm" (communal variable)
        'a_comdef': "",

        # "align" keyword
        'a_align': "align",

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

        # size of type (format string)
        'a_sizeof_fmt': "size %s",
    } # Assembler

    # ----------------------------------------------------------------------
    # Some internal flags used by the decoder, emulator and output
    #
    FL_B               = 0x000000001 # 8 bits
    FL_W               = 0x000000002 # 16 bits
    FL_D               = 0x000000004 # 32 bits
    FL_Q               = 0x000000008 # 64 bits
    FL_OP1             = 0x000000010 # check operand 1
    FL_32              = 0x000000020 # Is 32
    FL_64              = 0x000000040 # Is 64
    FL_NATIVE          = 0x000000080 # native call (not EbcCal)
    FL_REL             = 0x000000100 # relative address
    FL_CS              = 0x000000200 # Condition flag is set
    FL_NCS             = 0x000000400 # Condition flag is not set
    FL_INDIRECT        = 0x000000800 # This is an indirect access (not immediate value)
    FL_SIGNED          = 0x000001000 # This is a signed operand

    # ----------------------------------------------------------------------
    # Utility functions
    #

    # ----------------------------------------------------------------------
    def get_data_width_fl(self, sz):
        """Returns a flag given the data width number"""
        if   sz == 0: return self.FL_B
        elif sz == 1: return self.FL_W
        elif sz == 2: return self.FL_D
        elif sz == 3: return self.FL_Q

    # ----------------------------------------------------------------------
    def next_data_value(self, sz):
        """Returns a value depending on the data widh number"""
        if   sz == 0: return ua_next_byte()
        elif sz == 1: return ua_next_word()
        elif sz == 2: return ua_next_long()
        elif sz == 3: return ua_next_qword()
        else: raise Exception, "Invalid width!"

    # ----------------------------------------------------------------------
    def get_data_dt(self, sz):
        """Returns a dt_xxx on the data widh number"""
        if   sz == 0: return dt_byte
        elif sz == 1: return dt_word
        elif sz == 2: return dt_dword
        elif sz == 3: return dt_qword
        else: raise Exception, "Invalid width!"

    # ----------------------------------------------------------------------
    def get_sz_to_bits(self, sz):
        """Returns size in bits of the data widh number"""
        if   sz == 1: return 16
        elif sz == 2: return 32
        elif sz == 3: return 64
        else:         return 8

    # ----------------------------------------------------------------------
    def dt_to_bits(self, dt):
        """Returns the size in bits given a dt_xxx"""
        if   dt == dt_byte:  return 8
        elif dt == dt_word:  return 16
        elif dt == dt_dword: return 32
        elif dt == dt_qword: return 64

    # ----------------------------------------------------------------------
    def fl_to_str(self, fl):
        """Given a flag, it returns a string. (used during output)"""
        if   fl & self.FL_B != 0: return "B"
        elif fl & self.FL_W != 0: return "W"
        elif fl & self.FL_D != 0: return "D"
        elif fl & self.FL_Q != 0: return "Q"

    # ----------------------------------------------------------------------
    def set_ptr_size(self):
        """
        Functions checks the database / PE header netnode and sees if the input file
        is a PE+ or not. If so, then pointer size is set to 8 bytes.
        """
        n = netnode("$ PE header")
        s = n.valobj()
        if not s:
            return
        # Extract magic field
        t = struct.unpack("H", s[0x18:0x18+2])[0]
        if t == 0x20B:
            self.PTRSZ = 8

    # ----------------------------------------------------------------------
    # Decodes an index and returns all its components in a dictionary
    # Refer to "Index Encoding" section
    def decode_index(self, index, sz):
        bn = sz - 1
        s = -1 if copy_bits(index, bn) == 1 else 1
        w = copy_bits(index, bn-3, bn-1)
        if sz == 16  : t = 2
        elif sz == 32: t = 4
        elif sz == 64: t = 8

        a  = w * t # actual width
        c  = copy_bits(index, a, bn-4) # constant number
        n  = copy_bits(index, 0, a-1) # natural number
        o  = (c + (n * self.PTRSZ)) # offset w/o sign
        so = o * s # signed final offset

        # return all the components of an index
        r  = {'s': s, 'w': w, 'a': a, 'c': c, 'n': n, 'o': o, 'so': so}
        return r

    # ----------------------------------------------------------------------
    # Instruction decoding
    #

    # ----------------------------------------------------------------------
    def decode_RET(self, opbyte):
        # No operands
        self.cmd.Op1.type  = o_void
        # Consume the next byte, and it should be zero
        ua_next_byte()
        return True

    # ----------------------------------------------------------------------
    def decode_STORESP(self, opbyte):
        # opbyte (byte0) has nothing meaningful (but the opcode itself)

        # get next byte
        opbyte = ua_next_byte()

        vm_reg = (opbyte & 0x70) >> 4
        gp_reg = (opbyte & 0x07)

        self.cmd.Op1.type = self.cmd.Op2.type = o_reg
        self.cmd.Op1.dtyp = self.cmd.Op2.dtyp = dt_qword

        self.cmd.Op1.reg  = gp_reg
        self.cmd.Op2.reg  = self.ireg_FLAGS + vm_reg

        return True

    # ----------------------------------------------------------------------
    def decode_LOADSP(self, opbyte):
        # opbyte (byte0) has nothing meaningful (but the opcode itself)

        # get next byte
        opbyte = ua_next_byte()

        gp_reg = (opbyte & 0x70) >> 4
        vm_reg = (opbyte & 0x07)

        self.cmd.Op1.type = self.cmd.Op2.type = o_reg
        self.cmd.Op1.dtyp = self.cmd.Op2.dtyp = dt_qword

        self.cmd.Op1.reg  = self.ireg_FLAGS + vm_reg
        self.cmd.Op2.reg  = gp_reg

        return True

    # ----------------------------------------------------------------------
    def decode_BREAK(self, opbyte):
        """
        stx=<BREAK [break code]>
        txt=<The BREAK instruction is used to perform special processing by the VM.>
        """
        self.cmd.Op1.type  = o_imm
        self.cmd.Op1.dtyp  = dt_byte
        self.cmd.Op1.value = ua_next_byte()
        return True

    # ----------------------------------------------------------------------
    def decode_PUSH(self, opbyte):
        """
        stx=<PUSH[32|64] {@}R1 {Index16|ImmeDBG_FLAG6}>
        """
        have_data     = (opbyte & 0x80) != 0
        is_n          = (opbyte & ~0xC0) in [0x35, 0x36]
        op_32         = False if is_n else (opbyte & 0x40) == 0

        opbyte = ua_next_byte()

        op1_direct    = (opbyte & 0x08) == 0
        r1            = (opbyte & 0x07)
        self.cmd.Op1.dtyp  = dt_dword if op_32 else dt_qword

        fl = 0
        if have_data:
            d = ua_next_word()
            if not op1_direct:
                fl |= self.FL_INDIRECT
                d = self.decode_index(d, 16)['so']
            else:
                d = as_signed(d, 16)
            self.cmd.Op1.type    = o_displ
            self.cmd.Op1.phrase  = r1
            self.cmd.Op1.addr    = d
            self.cmd.Op1.specval = fl
        else:
            self.cmd.Op1.type    = o_reg
            self.cmd.Op1.reg     = r1

        self.cmd.auxpref = 0 if is_n else self.FL_32 if op_32 else self.FL_64
        return True

    # ----------------------------------------------------------------------
    def decode_JMP(self, opbyte):
        """
        stx=<JMP32{cs|cc} {@}R1 {Immed32|Index32}>
        stx=<JMP64{cs|cc} Immed64>
        """
        have_data     = (opbyte & 0x80) != 0
        jmp_32        = (opbyte & 0x40) == 0

        opbyte        = ua_next_byte()
        conditional   = (opbyte & 0x80) != 0
        cs            = (opbyte & 0x40) != 0
        abs_jmp       = (opbyte & 0x10) == 0
        op1_direct    = (opbyte & 0x08) == 0
        r1            = (opbyte & 0x07)

        fl = 0 if abs_jmp else self.FL_REL
        if jmp_32:
            # Indirect and no data specified?
            if not op1_direct and not have_data:
                return False

            if have_data:
                d = self.next_data_value(2) # 32-bits

                if r1 == 0:
                    self.cmd.Op1.type   = o_near
                else:
                    self.cmd.Op1.type   = o_displ
                    self.cmd.Op1.phrase = r1

                self.cmd.Op1.dtyp = dt_dword

                if not op1_direct:
                    d = self.decode_index(d, 32)['so']
                    fl |= self.FL_INDIRECT
                else:
                    d = as_signed(d, 32)

                if not abs_jmp:
                    d += self.cmd.ea + self.cmd.size

                self.cmd.Op1.addr    = d
                self.cmd.Op1.specval = fl
            else:
                self.cmd.Op1.type = o_reg
                self.cmd.Op1.reg  = r1
        else:
            self.cmd.Op1.type  = o_near
            self.cmd.Op1.dtyp  = dt_qword
            self.cmd.Op1.addr  = ua_next_qword()

        self.cmd.Op1.specval = fl

        fl = self.FL_32 if jmp_32 else self.FL_64
        if conditional:
            fl |= self.FL_CS if cs else self.FL_NCS

        self.cmd.auxpref = fl

        return True

    # ----------------------------------------------------------------------
    def decode_JMP8(self, opbyte):
        """
        stx=<JMP8{cs|cc} Immed8>
        """
        conditional   = (opbyte & 0x80) != 0
        cs            = (opbyte & 0x40) != 0
        self.cmd.Op1.type  = o_near
        self.cmd.Op1.dtyp  = dt_byte
        addr          = ua_next_byte()
        self.cmd.Op1.addr  = (as_signed(addr, 8) * 2) + self.cmd.size + self.cmd.ea

        if conditional:
            self.cmd.auxpref = self.FL_CS if cs else self.FL_NCS

        return True

    # ----------------------------------------------------------------------
    def decode_MOVI(self, opbyte):
        """
        txt=<Move a signed immediate value to operand 1>
        stx=<MOVI[b|w|d|q][w|d|q] {@}R1 {Index16}, ImmeDBG_FLAG6|32|64]>

        First character specifies the width of the move and is taken from r1
        Second character specifies the immediate data size and is taken from the opbyte
        """
        imm_sz   = (opbyte & 0xC0) >> 6
        opcode   = (opbyte & ~0xC0)
        is_MOVIn = opcode == 0x38

        # Reserved and should not be 0
        if imm_sz == 0:
            return False

        # take byte 1
        opbyte = ua_next_byte()

        # Bit 7 is reserved and should be 0
        if opbyte & 0x80 != 0:
            return False

        have_idx = (opbyte & 0x40) != 0
        move_sz  = (opbyte & 0x30) >> 4
        direct   = (opbyte & 0x08) == 0
        r1       = (opbyte & 0x07)

        # Cannot have an index with a direct register
        if have_idx and direct:
            return False

        if is_MOVIn:
            self.cmd.Op1.dtyp    = self.get_data_dt(imm_sz)
        else:
            self.cmd.Op1.specval = self.get_data_width_fl(move_sz)
            self.cmd.Op1.dtyp    = self.get_data_dt(move_sz)

        if have_idx:
            self.cmd.Op1.type   = o_displ
            self.cmd.Op1.phrase = r1
            self.cmd.Op1.addr   = self.decode_index(ua_next_word(), 16)['so']
        else:
            self.cmd.Op1.type = o_reg
            self.cmd.Op1.reg  = r1

        self.cmd.Op2.type  = o_imm
        self.cmd.Op2.dtyp  = self.get_data_dt(imm_sz)
        d = self.next_data_value(imm_sz)
        if is_MOVIn:
            d = self.decode_index(d, self.get_sz_to_bits(imm_sz))['so']

        self.cmd.Op2.value = d
        # save imm size and signal that op1 is defined in first operand
        self.cmd.auxpref   = self.get_data_width_fl(imm_sz) | (0 if is_MOVIn else self.FL_OP1)

        return True

    # ----------------------------------------------------------------------
    def decode_MOVREL(self, opbyte):
        imm_sz = (opbyte & 0xC0) >> 6

        # Reserved and should not be 0
        if imm_sz == 0:
            return False

        # take byte 1
        opbyte = ua_next_byte()

        # Bit 7 is reserved and should be 0
        if opbyte & 0x80 != 0:
            return False

        have_idx = (opbyte & 0x40) != 0
        direct   = (opbyte & 0x08) == 0
        r1       = (opbyte & 0x07)

        # Cannot have an index with a direct register
        if have_idx and direct:
            return False

        self.cmd.Op1.specval = self.get_data_width_fl(imm_sz)
        if have_idx:
            self.cmd.Op1.type   = o_displ
            self.cmd.Op1.phrase = r1
            self.cmd.Op1.addr   = self.decode_index(ua_next_word(), 16)['so']
        else:
            self.cmd.Op1.type   = o_reg
            self.cmd.Op1.reg    = r1

        self.cmd.Op2.type   = o_mem
        self.cmd.Op2.dtyp   = self.get_data_dt(imm_sz)
        self.cmd.Op2.addr   = self.next_data_value(imm_sz) + self.cmd.size + self.cmd.ea

        # save imm size
        self.cmd.auxpref   = self.get_data_width_fl(imm_sz)

        return True

    # ----------------------------------------------------------------------
    def decode_MOV(self, opbyte):
        have_idx1 = (opbyte & 0x80) != 0
        have_idx2 = (opbyte & 0x40) != 0
        opcode    = (opbyte & ~0xC0)

        # MOVxW
        if 0x1D <= opcode <= 0x20:
            idx_sz  = 1 # word
            data_sz = opcode - 0x1D
        # MOVxD
        elif 0x21 <= opcode <= 0x24:
            idx_sz  = 2 # dword
            data_sz = opcode - 0x21
        # MOVqq
        elif opcode == 0x28:
            idx_sz  = 3 # qword
            data_sz = 3
        # MOVnw
        elif opcode == 0x32:
            idx_sz  = 1 # word
            data_sz = 3
        # MOVnd
        elif opcode == 0x33:
            idx_sz  = 2 # dword
            data_sz = 3

        # get byte1
        opbyte = ua_next_byte()

        op2_direct = (opbyte & 0x80) == 0
        op1_direct = (opbyte & 0x08) == 0
        r1         = (opbyte & 0x07)
        r2         = (opbyte & 0x70) >> 4

        # indirect
        fl = self.FL_SIGNED
        if have_idx1:
            d = self.decode_index(self.next_data_value(idx_sz), self.get_sz_to_bits(idx_sz))['so']
            if not op1_direct:
                fl |= self.FL_INDIRECT
            self.cmd.Op1.type   = o_displ
            self.cmd.Op1.phrase = r1
            self.cmd.Op1.addr   = d
            self.cmd.Op1.dtyp   = self.get_data_dt(idx_sz)
        else:
            self.cmd.Op1.type   = o_reg
            self.cmd.Op1.reg    = r1
            self.cmd.Op1.dtyp   = self.get_data_dt(data_sz)

        self.cmd.Op1.specval = fl

        fl = self.FL_SIGNED
        if have_idx2:
            d = self.decode_index(self.next_data_value(idx_sz), self.get_sz_to_bits(idx_sz))['so']
            if not op2_direct:
                fl |= self.FL_INDIRECT
            self.cmd.Op2.type   = o_displ
            self.cmd.Op2.phrase = r2
            self.cmd.Op2.addr   = d
            self.cmd.Op2.dtyp   = self.get_data_dt(idx_sz)
        else:
            self.cmd.Op2.type   = o_reg
            self.cmd.Op2.reg    = r2
            self.cmd.Op2.dtyp   = self.get_data_dt(data_sz)

        self.cmd.Op2.specval = fl

        return True

    # ----------------------------------------------------------------------
    def decode_CMP(self, opbyte):
        have_data  = (opbyte & 0x80) != 0
        cmp_32     = (opbyte & 0x40) == 0
        opcode     = (opbyte & ~0xC0)

        opbyte     = ua_next_byte()

        op2_direct = (opbyte & 0x80) == 0
        r1         = (opbyte & 0x07)
        r2         = (opbyte & 0x70) >> 4

        dt = dt_dword if cmp_32 else dt_qword
        self.cmd.Op1.type   = o_reg
        self.cmd.Op1.reg    = r1
        self.cmd.Op1.dtyp   = dt

        if have_data:
            self.cmd.Op2.type   = o_displ
            self.cmd.Op2.phrase = r2
            self.cmd.Op2.dtyp   = dt
            addr           = self.next_data_value(1) # get 16bit value
            if not op2_direct:
                addr = self.decode_index(addr, 16)['so']
                self.cmd.Op2.specval = self.FL_INDIRECT
            self.cmd.Op2.addr = addr
        else:
            self.cmd.Op2.type = o_reg
            self.cmd.Op2.reg  = r2

        self.cmd.auxpref = self.FL_32 if cmp_32 else self.FL_64
        return True

    # ----------------------------------------------------------------------
    def decode_CMPI(self, opbyte):
        """
        stx=<CMPI[32|64]{w|d}[eq|lte|gte|ulte|ugte] {@}R1 {Index16}, ImmeDBG_FLAG6|Immed32>
        """
        imm_sz    = 1 if (opbyte & 0x80) == 0 else 2
        cmp_32    = (opbyte & 0x40) == 0
        opcode    = (opbyte & ~0xC0)

        opbyte = ua_next_byte()

        have_idx   = (opbyte & 0x10) != 0
        op1_direct = (opbyte & 0x08) == 0
        r1         = (opbyte & 0x07)
        dt         = self.get_data_dt(imm_sz)

        if op1_direct:
            self.cmd.Op1.type = o_reg
            self.cmd.Op1.reg  = r1
            self.cmd.Op1.dtyp = dt
        else:
            if have_idx:
                d = self.decode_index(ua_next_word(), 16)['so']
            else:
                d = 0
            self.cmd.Op1.type   = o_displ
            self.cmd.Op1.phrase = r1
            self.cmd.Op1.addr   = d

        self.cmd.Op2.type  = o_imm
        self.cmd.Op2.value = self.next_data_value(imm_sz)
        self.cmd.Op2.dtyp  = dt

        self.cmd.auxpref    = (self.FL_32 if cmp_32 else self.FL_64) | self.get_data_width_fl(imm_sz)
        return True

    # ----------------------------------------------------------------------
    def decode_CALL(self, opbyte):
        """
        stx=<CALL32{EX}{a} {@}R1 {Immed32|Index32}>
        stx=<CALL64{EX}{a} Immed64>
        """
        have_data  = (opbyte & 0x80) != 0
        call_32    = (opbyte & 0x40) == 0

        opbyte     = ua_next_byte()

        # Call to EBC or Native code
        ebc_call   = (opbyte & 0x20) == 0
        # Absolute or Relative address
        abs_addr   = (opbyte & 0x10) == 0
        # Op1 direct?
        op1_direct = (opbyte & 0x08) == 0
        r1         = (opbyte & 0x07)

        fl = 0
        if call_32:
            if have_data:
                addr = self.next_data_value(2)
                # Indirect
                if not op1_direct:
                    addr = self.decode_index(addr, 32)['so']
                    fl |= self.FL_INDIRECT

                self.cmd.Op1.dtyp = dt_dword

                if r1 == 0:
                    self.cmd.Op1.type   = o_near
                else:
                    self.cmd.Op1.type   = o_displ
                    self.cmd.Op1.phrase = r1

                if not abs_addr:
                    addr = self.cmd.ea + as_signed(addr, 32) + self.cmd.size
                self.cmd.Op1.addr = addr
            else:
                self.cmd.Op1.type = o_reg
                self.cmd.Op1.reg  = r1
        # 64-bit
        else:
            self.cmd.Op1.type = o_mem
            self.cmd.Op1.dtyp = dt_qword
            self.cmd.Op1.addr = self.next_data_value(3) # Get 64-bit value

        if not ebc_call: fl |= self.FL_NATIVE
        if not abs_addr: fl |= self.FL_REL

        self.cmd.Op1.specval = fl

        fl  = self.FL_NATIVE if not ebc_call else 0
        fl |= self.FL_32 if call_32 else self.FL_64
        self.cmd.auxpref = fl

        return True

    # ----------------------------------------------------------------------
    def decode_BINOP_FORM1(self, opbyte):
        have_data  = (opbyte & 0x80) != 0
        op_32      = (opbyte & 0x40) == 0
        opcode     = (opbyte & ~0xC0)

        opbyte     = ua_next_byte()

        op2_direct = (opbyte & 0x80) == 0
        op1_direct = (opbyte & 0x08) == 0
        r1         = (opbyte & 0x07)
        r2         = (opbyte & 0x70) >> 4
        dt         = dt_dword if op_32 else dt_qword

        # handle operand 2
        if have_data:
            self.cmd.Op2.dtyp = dt
            d = self.next_data_value(1) # one = imm16
            if not op2_direct:
                d = self.decode_index(d, 16)['so']
                self.cmd.Op2.specval = self.FL_INDIRECT

            self.cmd.Op2.type   = o_displ
            self.cmd.Op2.phrase = r2
            self.cmd.Op2.addr   = d
        else:
            self.cmd.Op2.type   = o_reg
            self.cmd.Op2.reg    = r2

        # handle operand 1
        if op1_direct:
            self.cmd.Op1.type   = o_reg
            self.cmd.Op1.reg    = r1
            self.cmd.Op1.dtyp   = dt
        else:
            self.cmd.Op1.type   = o_displ
            self.cmd.Op1.phrase = r1
            self.cmd.Op1.addr   = 0
            self.cmd.Op1.specval = self.FL_INDIRECT

        self.cmd.auxpref = self.FL_32 if op_32 else self.FL_64

        return True

    # ----------------------------------------------------------------------
    def decode_MOVSN(self, opbyte):
        have_idx1  = (opbyte & 0x80) != 0
        have_idx2  = (opbyte & 0x40) != 0
        opcode     = (opbyte & ~0xC0)

        if opcode == 0x25:
            idx_sz = 1
        elif opcode == 0x26:
            idx_sz = 2
        else:
            return False

        opbyte = ua_next_byte()
        op2_direct = (opbyte & 0x80) == 0
        op1_direct = (opbyte & 0x08) == 0
        r1         = (opbyte & 0x07)
        r2         = (opbyte & 0x70) >> 4
        dt         = self.get_data_dt(idx_sz)

        if have_idx1:
            self.cmd.Op1.type    = o_displ
            self.cmd.Op1.phrase  = r1
            self.cmd.Op1.addr    = self.decode_index(self.next_data_value(idx_sz), self.get_sz_to_bits(idx_sz))['so']
        else:
            self.cmd.Op1.type   = o_reg
            self.cmd.Op1.reg    = r1

        if have_idx2:
            d = self.next_data_value(idx_sz)
            self.cmd.Op2.type   = o_displ
            self.cmd.Op2.phrase = r2
            if not op2_direct:
                d = self.decode_index(d, self.get_sz_to_bits(idx_sz))['so']
                self.cmd.Op2.specval = self.FL_INDIRECT
            self.cmd.Op2.addr = d
        else:
            self.cmd.Op2.type   = o_reg
            self.cmd.Op2.reg    = r2

        return True

    # ----------------------------------------------------------------------
    # Processor module callbacks
    #

    # ----------------------------------------------------------------------
    def is_align_insn(self, ea):
        """
        Is the instruction created only for alignment purposes?
        Returns: number of bytes in the instruction
        """
        return 2 if get_word(ea) == 0 else 0

    # ----------------------------------------------------------------------
    def notify_newfile(self, filename):
        self.set_ptr_size()

    # ----------------------------------------------------------------------
    def notify_oldfile(self, filename):
        self.set_ptr_size()

    # ----------------------------------------------------------------------
    def handle_operand(self, op, isRead):
        uFlag     = self.get_uFlag()
        is_offs   = isOff(uFlag, op.n)
        dref_flag = dr_R if isRead else dr_W
        def_arg   = isDefArg(uFlag, op.n)
        optype    = op.type

        # create code xrefs
        if optype == o_imm:
            if is_offs:
                ua_add_off_drefs(op, dr_O)
        # create data xrefs
        elif optype == o_displ:
            if is_offs:
                ua_add_off_drefs(op, dref_flag)
        elif optype == o_mem:
            ua_add_dref(op.offb, op.addr, dref_flag)
        elif optype == o_near:
            itype = self.cmd.itype
            if itype == self.itype_CALL:
                fl = fl_CN
            else:
                fl = fl_JN
            ua_add_cref(op.offb, op.addr, fl)

    # ----------------------------------------------------------------------
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
        uncond_jmp = self.cmd.itype in [self.itype_JMP8, self.itype_JMP] and (aux & (self.FL_NCS|self.FL_CS)) == 0

        # add flow
        if (Feature & CF_STOP == 0) and not uncond_jmp:
            ua_add_cref(0, self.cmd.ea + self.cmd.size, fl_F)

        return 1

    # ----------------------------------------------------------------------
    def outop(self, op):
        """
        Generate text representation of an instructon operand.
        This function shouldn't change the database, flags or anything else.
        All these actions should be performed only by u_emu() function.
        The output text is placed in the output buffer initialized with init_output_buffer()
        This function uses out_...() functions from ua.hpp to generate the operand text
        Returns: 1-ok, 0-operand is hidden.
        """
        optype = op.type
        fl     = op.specval
        signed = OOF_SIGNED if fl & self.FL_SIGNED != 0 else 0

        if optype == o_reg:
            out_register(self.regNames[op.reg])

        elif optype == o_imm:
            OutValue(op, OOFW_IMM | signed | (OOFW_32 if self.PTRSZ == 4 else OOFW_64))

        elif optype in [o_near, o_mem]:
            r = out_name_expr(op, op.addr, BADADDR)
            if not r:
                out_tagon(COLOR_ERROR)
                OutLong(op.addr, 16)
                out_tagoff(COLOR_ERROR)
                QueueMark(Q_noName, self.cmd.ea)

        elif optype == o_displ:
            indirect = fl & self.FL_INDIRECT != 0
            if indirect:
                out_symbol('[')

            out_register(self.regNames[op.reg])

            if op.addr != 0:
                OutValue(op, OOF_ADDR | OOFW_16 | signed | OOFS_NEEDSIGN)

            if indirect:
                out_symbol(']')
        else:
            return False

        return True

    # ----------------------------------------------------------------------
    # Generate text representation of an instruction in 'cmd' structure.
    # This function shouldn't change the database, flags or anything else.
    # All these actions should be performed only by u_emu() function.
    def out(self):
        # Init output buffer
        buf = idaapi.init_output_buffer(1024)

        postfix = ""

        # First display size of first operand if it exists
        if self.cmd.auxpref & self.FL_OP1 != 0:
            postfix += self.fl_to_str(self.cmd.Op1.specval)

        # Display opertion size
        if self.cmd.auxpref & self.FL_32:
            postfix += "32"
        elif self.cmd.auxpref & self.FL_64:
            postfix += "64"

        # Display if native or not native (for CALL)
        if self.cmd.auxpref & self.FL_NATIVE:
            postfix += "EX"

        # Display size of instruction
        if self.cmd.auxpref & (self.FL_B | self.FL_W | self.FL_D | self.FL_Q) != 0:
            postfix += self.fl_to_str(self.cmd.auxpref)

        if self.cmd.auxpref & self.FL_CS:
            postfix += "CS"
        elif self.cmd.auxpref & self.FL_NCS:
            postfix += "CC"

        OutMnem(15, postfix)

        out_one_operand( 0 )

        for i in xrange(1, 3):
            op = self.cmd[i]

            if op.type == o_void:
                break

            out_symbol(',')
            OutChar(' ')
            out_one_operand(i)

        term_output_buffer()

        cvar.gl_comm = 1
        MakeLine(buf)

    # ----------------------------------------------------------------------
    def ana(self):
        """
        Decodes an instruction into the C global variable 'cmd'
        """

        # take opcode byte
        b = ua_next_byte()

        # the 6bit opcode
        opcode = b & 0x3F

        # opcode supported?
        try:
            ins = self.itable[opcode]
        except:
            return 0

        # set default itype
        self.cmd.itype = getattr(self, 'itype_' + ins.name)

        # call the decoder
        return self.cmd.size if ins.d(b) else 0

    # ----------------------------------------------------------------------
    def init_instructions(self):
        class idef:
            """
            Internal class that describes an instruction by:
            - instruction name
            - instruction decoding routine
            - canonical flags used by IDA
            """
            def __init__(self, name, cf, d):
                self.name = name
                self.cf  = cf
                self.d   = d

        #
        # Instructions table (w/ pointer to decoder)
        #
        self.itable = {
            0x00: idef(name='BREAK',        d=self.decode_BREAK, cf = CF_USE1),

            0x01: idef(name='JMP',          d=self.decode_JMP, cf = CF_USE1 | CF_JUMP),
            0x02: idef(name='JMP8',         d=self.decode_JMP8, cf = CF_USE1 | CF_JUMP),

            0x03: idef(name='CALL',         d=self.decode_CALL, cf = CF_USE1 | CF_CALL),
            0x04: idef(name='RET',          d=self.decode_RET, cf = CF_STOP),
            0x05: idef(name='CMPEQ',        d=self.decode_CMP, cf = CF_USE1 | CF_USE2),
            0x06: idef(name='CMPLTE',       d=self.decode_CMP, cf = CF_USE1 | CF_USE2),
            0x07: idef(name='CMPGTE',       d=self.decode_CMP, cf = CF_USE1 | CF_USE2),
            0x08: idef(name='CMPULTE',      d=self.decode_CMP, cf = CF_USE1 | CF_USE2),
            0x09: idef(name='CMPGTE',       d=self.decode_CMP, cf = CF_USE1 | CF_USE2),

            0x0A: idef(name='NOT',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x0B: idef(name='NEG',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x0C: idef(name='ADD',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x0D: idef(name='SUB',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x0E: idef(name='MUL',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x0F: idef(name='MULU',         d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x10: idef(name='DIV',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x11: idef(name='DIVU',         d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x12: idef(name='MOD',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x13: idef(name='MODU',         d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x14: idef(name='AND',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x15: idef(name='OR',           d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x16: idef(name='XOR',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x17: idef(name='SHL',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1 | CF_SHFT),
            0x18: idef(name='SHR',          d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1 | CF_SHFT),
            0x19: idef(name='ASHR',         d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1 | CF_SHFT),

            0x1A: idef(name='EXTNDB',       d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x1B: idef(name='EXTNDW',       d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x1C: idef(name='EXTNDD',       d=self.decode_BINOP_FORM1, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            0x1D: idef(name='MOVBW',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x1E: idef(name='MOVWW',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x1F: idef(name='MOVDW',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x20: idef(name='MOVQW',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x21: idef(name='MOVBD',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x22: idef(name='MOVWD',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x23: idef(name='MOVDD',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x24: idef(name='MOVQD',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            0x25: idef(name='MOVSNW',       d=self.decode_MOVSN, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x26: idef(name='MOVSND',       d=self.decode_MOVSN, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            #  0x27: reserved

            0x28: idef(name='MOVQQ',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            0x29: idef(name='LOADSP',       d=self.decode_LOADSP, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x2A: idef(name='STORESP',      d=self.decode_STORESP, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            # PUSH/POP
            0x2B: idef(name='PUSH',         d=self.decode_PUSH, cf = CF_USE1),
            0x2C: idef(name='POP',          d=self.decode_PUSH, cf = CF_USE1),

            # CMPI
            0x2D: idef(name='CMPIEQ',       d=self.decode_CMPI, cf = CF_USE1 | CF_USE2),
            0x2E: idef(name='CMPILTE',      d=self.decode_CMPI, cf = CF_USE1 | CF_USE2),
            0x2F: idef(name='CMPIGTE',      d=self.decode_CMPI, cf = CF_USE1 | CF_USE2),
            0x30: idef(name='CMPIULTE',     d=self.decode_CMPI, cf = CF_USE1 | CF_USE2),
            0x31: idef(name='CMPIUGTE',     d=self.decode_CMPI, cf = CF_USE1 | CF_USE2),

            0x32: idef(name='MOVNW',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x33: idef(name='MOVND',        d=self.decode_MOV, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            #  0x34: reserved

            # PUSHn/POPn
            0x35: idef(name='PUSHN',        d=self.decode_PUSH, cf = CF_USE1),
            0x36: idef(name='PUSHN',        d=self.decode_PUSH, cf = CF_USE1),

            0x37: idef(name='MOVI',         d=self.decode_MOVI, cf = CF_USE1 | CF_USE2 | CF_CHG1),
            0x38: idef(name='MOVIN',        d=self.decode_MOVI, cf = CF_USE1 | CF_USE2 | CF_CHG1),

            0x39: idef(name='MOVREL',       d=self.decode_MOVREL, cf = CF_USE1 | CF_USE2 | CF_CHG1)
            #  0x3A: reserved
            #  0x3B: reserved
            #  0x3C: reserved
            #  0x3D: reserved
            #  0x3E: reserved
            #  0x3F: reserved
        }

        # Now create an instruction table compatible with IDA processor module requirements
        Instructions = []
        i = 0
        for x in self.itable.values():
            Instructions.append({'name': x.name, 'feature': x.cf})
            setattr(self, 'itype_' + x.name, i)
            i += 1

        # icode of the last instruction + 1
        self.instruc_end = len(Instructions) + 1

        # Array of instructions
        self.instruc = Instructions

        # Icode of return instruction. It is ok to give any of possible return
        # instructions
        self.icode_return = self.itype_RET

    # ----------------------------------------------------------------------
    def init_registers(self):
        """This function parses the register table and creates corresponding ireg_XXX constants"""

        # Registers definition
        self.regNames = [
            # General purpose registers
            "SP", # aka R0
            "R1",
            "R2",
            "R3",
            "R4",
            "R5",
            "R6",
            "R7",
            # VM registers
            "FLAGS", # 0
            "IP",    # 1
            "VM2",
            "VM3",
            "VM4",
            "VM5",
            "VM6",
            "VM7",
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
        self.PTRSZ = 4 # Assume PTRSZ = 4 by default
        self.init_instructions()
        self.init_registers()

# ----------------------------------------------------------------------
def PROCESSOR_ENTRY():
    return ebc_processor_t()
