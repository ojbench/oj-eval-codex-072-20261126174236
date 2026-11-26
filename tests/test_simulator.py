"""Deterministic instruction and pipeline regression tests, using only stdlib."""
import random
import subprocess
import sys

EXECUTABLE = sys.argv[1] if len(sys.argv) > 1 else "./build/code"
MASK = (1 << 32) - 1
HALT = 0x0FF00513


def immediate(rd, rs, value, funct=0, opcode=0x13):
    return ((value & 4095) << 20) | (rs << 15) | (funct << 12) | (rd << 7) | opcode


def register(rd, rs1, rs2, funct=0, upper=0):
    return (upper << 25) | (rs2 << 20) | (rs1 << 15) | (funct << 12) | (rd << 7) | 0x33


def store(rs, base, offset, funct):
    offset &= 4095
    return ((offset >> 5) << 25) | (rs << 20) | (base << 15) | (funct << 12) | ((offset & 31) << 7) | 0x23


def branch(rs1, rs2, offset, funct):
    offset &= 8191
    return ((offset >> 12) << 31) | (((offset >> 5) & 63) << 25) | (rs2 << 20) | (rs1 << 15) | (funct << 12) | (((offset >> 1) & 15) << 8) | (((offset >> 11) & 1) << 7) | 0x63


def signed(value):
    return value if value < 1 << 31 else value - (1 << 32)


def check(instructions, expected):
    data = "@00000000\n" + " ".join(f"{byte:02x}" for ins in instructions + [HALT] for byte in ins.to_bytes(4, "little"))
    result = subprocess.run([EXECUTABLE], input=data, text=True, capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == str(expected & 255), (expected & 255, result.stdout)


# RAW dependencies, x0, load/store widths and sign extension.
check([immediate(10, 0, 42)], 42)
check([immediate(0, 0, 123), immediate(10, 0, 0)], 0)
for width in range(3):
    for unsigned in (False, True):
        if width == 2 and unsigned:
            continue
        check([immediate(1, 0, 1024), immediate(2, 0, -128),
               store(2, 1, 3, width), immediate(3, 1, 3, width + 4 * unsigned, 3),
               immediate(10, 3, 8, 5)], 0 if unsigned and width == 0 else 255)

# Every branch condition, including wrong-path termination and a backward loop.
for funct in (0, 1, 4, 5, 6, 7):
    for a, b in ((-1, 1), (1, -1), (1, 1)):
        ua, ub = a & MASK, b & MASK
        take = {0: a == b, 1: a != b, 4: a < b, 5: a >= b, 6: ua < ub, 7: ua >= ub}[funct]
        check([immediate(1, 0, a), immediate(2, 0, b), immediate(10, 0, 11),
               branch(1, 2, 8, funct), HALT, immediate(10, 0, 22)], 22 if take else 11)
check([immediate(1, 0, 100), immediate(10, 0, 0), immediate(10, 10, 1),
       immediate(1, 1, -1), branch(1, 0, -8, 1)], 100)

# JAL, JALR, AUIPC and LUI; jumps must flush a fetched halt.
check([0x008000EF, HALT, immediate(10, 1, 0)], 4)
check([immediate(1, 0, 13), immediate(2, 1, 0, 0, 0x67), HALT,
       immediate(10, 2, 0)], 8)
check([0x123450B7, 0x00000117, register(10, 1, 2)], 4)

# Compare long dependency-heavy arithmetic streams with an independent oracle.
rng = random.Random(1605)
for trial in range(100):
    regs = [0] * 32
    program = []
    for _ in range(500):
        rd, rs1, rs2 = (rng.randrange(32) for _ in range(3))
        funct = rng.randrange(8)
        a = regs[rs1]
        if rng.randrange(2):
            b = regs[rs2]
            upper = 32 if funct in (0, 5) and rng.randrange(2) else 0
            program.append(register(rd, rs1, rs2, funct, upper))
            subtract = upper != 0 and funct == 0
        else:
            value = rng.randrange(-2048, 2048)
            upper = 32 if funct == 5 and rng.randrange(2) else 0
            if funct in (1, 5):
                value = rng.randrange(32) | (upper << 5)
            b = value & MASK
            program.append(immediate(rd, rs1, value, funct))
            subtract = False
        value = [a - b if subtract else a + b,
                 a << (b & 31), int(signed(a) < signed(b)), int(a < b), a ^ b,
                 signed(a) >> (b & 31) if upper else a >> (b & 31), a | b, a & b][funct]
        if rd:
            regs[rd] = value & MASK
    # Fold all registers so the comparison covers more than a final assignment.
    for rs in range(1, 32):
        if rs != 10:
            program.append(register(10, 10, rs, 4))
            regs[10] ^= regs[rs]
    check(program, regs[10])
print("All instruction, hazard, control-flow, and 100 randomized tests passed.")
