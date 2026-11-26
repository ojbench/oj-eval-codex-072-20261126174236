#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Word = std::uint32_t;

static Word sign_extend(Word value, unsigned bits) {
    const Word sign = Word{1} << (bits - 1);
    return (value ^ sign) - sign;
}

static bool signed_less(Word a, Word b) {
    return (a ^ 0x80000000u) < (b ^ 0x80000000u);
}

static Word arithmetic_shift(Word a, unsigned shift) {
    shift &= 31;
    if (shift == 0) return a;
    return (a >> shift) | ((a & 0x80000000u) ? (~Word{0} << (32 - shift)) : 0);
}

class Memory {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(1 << 20);
public:
    Word read(Word address, unsigned size) const {
        Word value = 0;
        for (unsigned i = 0; i < size; ++i) {
            const std::uint64_t pos = std::uint64_t(address) + i;
            if (pos < bytes.size()) value |= Word(bytes[pos]) << (8 * i);
        }
        return value;
    }
    void write(Word address, unsigned size, Word value) {
        const std::uint64_t end = std::uint64_t(address) + size;
        if (end > (1u << 28)) throw std::runtime_error("Memory address out of range");
        if (end > bytes.size()) bytes.resize(std::max<std::uint64_t>(end, bytes.size() * 2));
        for (unsigned i = 0; i < size; ++i) bytes[address + i] = value >> (8 * i);
    }
    void load(std::istream &in) {
        std::string token;
        Word address = 0;
        while (in >> token) {
            if (token[0] == '@') address = std::stoul(token.substr(1), nullptr, 16);
            else write(address++, 1, std::stoul(token, nullptr, 16));
        }
    }
};

struct FetchDecode {
    bool valid = false;
    Word pc = 0, instruction = 0;
};

struct DecodeExecute {
    bool valid = false, halt = false;
    Word pc = 0, instruction = 0, a = 0, b = 0;
    unsigned rd = 0;
};

struct Result {
    bool valid = false, halt = false, load = false, store = false;
    unsigned rd = 0, size = 0;
    bool unsigned_load = false;
    Word value = 0, address = 0, store_value = 0;
};

class CPU {
    Memory memory;
    std::array<Word, 32> registers{};
    Word pc = 0;
    bool fetch_stopped = false;
    FetchDecode fd;
    DecodeExecute de;
    Result em, mw;

    static unsigned destination(Word instruction) {
        switch (instruction & 127) {
        case 0x37: case 0x17: case 0x6f: case 0x67:
        case 0x03: case 0x13: case 0x33: return (instruction >> 7) & 31;
        default: return 0;
        }
    }

    Result execute(bool &redirect, Word &target) const {
        Result out;
        if (!de.valid) return out;
        out.valid = true;
        out.halt = de.halt;
        out.rd = de.rd;
        if (out.halt) return out;
        const Word ins = de.instruction, a = de.a, b = de.b;
        const unsigned op = ins & 127, f3 = (ins >> 12) & 7;
        const Word imm = sign_extend(ins >> 20, 12);
        switch (op) {
        case 0x37: out.value = ins & 0xfffff000u; break;
        case 0x17: out.value = de.pc + (ins & 0xfffff000u); break;
        case 0x6f:
            out.value = de.pc + 4;
            target = de.pc + sign_extend(((ins >> 31) << 20) | (ins & 0xff000) |
                     ((ins >> 20 & 1) << 11) | ((ins >> 21 & 1023) << 1), 21);
            redirect = true;
            break;
        case 0x67:
            out.value = de.pc + 4;
            target = (a + imm) & ~Word{1};
            redirect = true;
            break;
        case 0x63: {
            bool taken = false;
            switch (f3) {
            case 0: taken = a == b; break;
            case 1: taken = a != b; break;
            case 4: taken = signed_less(a, b); break;
            case 5: taken = !signed_less(a, b); break;
            case 6: taken = a < b; break;
            case 7: taken = a >= b; break;
            }
            if (taken) {
                target = de.pc + sign_extend(((ins >> 31) << 12) | ((ins >> 7 & 1) << 11) |
                         ((ins >> 25 & 63) << 5) | ((ins >> 8 & 15) << 1), 13);
                redirect = true;
            }
            break;
        }
        case 0x03:
            out.load = true;
            out.address = a + imm;
            out.size = 1u << (f3 & 3);
            out.unsigned_load = f3 & 4;
            break;
        case 0x23:
            out.store = true;
            out.address = a + sign_extend(((ins >> 25) << 5) | (ins >> 7 & 31), 12);
            out.size = 1u << f3;
            out.store_value = b;
            break;
        case 0x13: case 0x33: {
            const Word rhs = op == 0x13 ? imm : b;
            switch (f3) {
            case 0: out.value = (op == 0x33 && (ins & (1u << 30))) ? a - rhs : a + rhs; break;
            case 1: out.value = a << (rhs & 31); break;
            case 2: out.value = signed_less(a, rhs); break;
            case 3: out.value = a < rhs; break;
            case 4: out.value = a ^ rhs; break;
            case 5: out.value = (ins & (1u << 30)) ? arithmetic_shift(a, rhs) : a >> (rhs & 31); break;
            case 6: out.value = a | rhs; break;
            case 7: out.value = a & rhs; break;
            }
            break;
        }
        case 0x0f: break; // In-order memory operations already satisfy FENCE.
        default: throw std::runtime_error("Unsupported instruction");
        }
        return out;
    }

public:
    explicit CPU(std::istream &in) { memory.load(in); }

    unsigned run() {
        for (;;) {
            // Each stage consumes only the previous cycle's pipeline registers.
            if (mw.valid) {
                if (mw.halt) return registers[10] & 255;
                if (mw.rd) registers[mw.rd] = mw.value;
            }
            Result next_mw = em;
            if (em.valid) {
                if (em.store) memory.write(em.address, em.size, em.store_value);
                if (em.load) {
                    next_mw.value = memory.read(em.address, em.size);
                    if (!em.unsigned_load && em.size < 4)
                        next_mw.value = sign_extend(next_mw.value, em.size * 8);
                }
            }
            bool redirect = false;
            Word target = 0;
            const Result next_em = execute(redirect, target);
            DecodeExecute next_de;
            bool stall = false;
            if (next_em.halt) fetch_stopped = true;
            if (redirect) pc = target;
            if (fd.valid && !redirect && !fetch_stopped) {
                const Word ins = fd.instruction;
                const unsigned op = ins & 127, rs1 = ins >> 15 & 31, rs2 = ins >> 20 & 31;
                const bool halt = ins == 0x0ff00513;
                const bool use1 = op == 0x67 || op == 0x63 || op == 0x03 || op == 0x23 || op == 0x13 || op == 0x33;
                const bool use2 = op == 0x63 || op == 0x23 || op == 0x33;
                auto pending = [&](unsigned reg) {
                    return reg && ((de.valid && de.rd == reg) || (em.valid && em.rd == reg));
                };
                stall = !halt && ((use1 && pending(rs1)) || (use2 && pending(rs2)));
                if (!stall) next_de = {true, halt, fd.pc, ins, registers[rs1], registers[rs2], halt ? 0 : destination(ins)};
            }
            if (!stall) {
                fd = {};
                if (!fetch_stopped) {
                    fd = {true, pc, memory.read(pc, 4)};
                    pc += 4;
                }
            }
            de = next_de;
            em = next_em;
            mw = next_mw;
        }
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    try {
        CPU cpu(std::cin);
        std::cout << cpu.run() << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
