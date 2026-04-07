// Demo 6: SystemC data types for hardware modeling
// ==================================================
// Key concepts:
//   - sc_uint<N> / sc_int<N>: fixed-width unsigned/signed integers (1-64 bits)
//   - sc_biguint<N> / sc_bigint<N>: arbitrary-width integers (>64 bits)
//   - sc_bv<N>: bit vector — no X/Z, just 0 and 1
//   - sc_lv<N>: logic vector — 4-valued: 0, 1, X (unknown), Z (high-impedance)
//   - sc_fixed<W,I>: fixed-point types for DSP modeling
//   - These types support bit selection, range, concatenation
//
// This demo: an ALU using fixed-width types, and a bus with tri-state logic.

#include <systemc.h>

// ---- ALU using sc_uint ----
SC_MODULE(ALU) {
    sc_in<sc_uint<8>>  a;
    sc_in<sc_uint<8>>  b;
    sc_in<sc_uint<2>>  op;     // 0=ADD, 1=SUB, 2=AND, 3=OR
    sc_out<sc_uint<8>> result;
    sc_out<bool>       carry;  // overflow bit

    void compute() {
        sc_uint<8> va = a.read();
        sc_uint<8> vb = b.read();
        sc_uint<9> tmp;  // 9 bits to capture carry

        switch (op.read().to_uint()) {
            case 0: tmp = va + vb; break;
            case 1: tmp = va - vb; break;
            case 2: tmp = va & vb; break;
            case 3: tmp = va | vb; break;
            default: tmp = 0;
        }

        result.write(tmp.range(7, 0));  // lower 8 bits
        carry.write(tmp[8]);            // bit selection: carry out

        std::cout << sc_time_stamp() << " ALU: "
                  << va.to_string(SC_BIN) << " op" << op.read()
                  << " " << vb.to_string(SC_BIN)
                  << " = " << tmp.range(7,0).to_string(SC_BIN)
                  << " carry=" << tmp[8] << std::endl;
    }

    SC_CTOR(ALU) {
        SC_METHOD(compute);
        sensitive << a << b << op;
    }
};

// ---- Demonstrate bit manipulation ----
SC_MODULE(BitOps) {
    void run() {
        std::cout << "\n--- Bit operations ---" << std::endl;

        // Bit selection and range
        sc_uint<8> val = 0b11010110;
        std::cout << "val         = " << val.to_string(SC_BIN) << std::endl;
        std::cout << "val[7]      = " << val[7] << "  (MSB)" << std::endl;
        std::cout << "val[0]      = " << val[0] << "  (LSB)" << std::endl;
        std::cout << "val(5,2)    = " << val.range(5,2).to_string(SC_BIN)
                  << "  (bits 5 downto 2)" << std::endl;

        // Concatenation using comma operator
        sc_uint<4> hi = 0xA;
        sc_uint<4> lo = 0x5;
        sc_uint<8> combined = (hi, lo);  // concatenation!
        std::cout << "concat(0xA, 0x5) = 0x" << std::hex
                  << combined.to_uint() << std::dec << std::endl;

        // sc_bv: bit vector (2-valued: 0 and 1)
        std::cout << "\n--- sc_bv (bit vector) ---" << std::endl;
        sc_bv<8> bv1 = "10110011";
        sc_bv<8> bv2 = "11001100";
        std::cout << "bv1     = " << bv1 << std::endl;
        std::cout << "bv2     = " << bv2 << std::endl;
        std::cout << "bv1 & bv2 = " << (bv1 & bv2) << std::endl;
        std::cout << "bv1 | bv2 = " << (bv1 | bv2) << std::endl;
        std::cout << "~bv1      = " << (~bv1) << std::endl;

        // sc_lv: logic vector (4-valued: 0, 1, X, Z)
        std::cout << "\n--- sc_lv (logic vector: 0, 1, X, Z) ---" << std::endl;
        sc_lv<8> lv1 = "10XZ10XZ";   // X=unknown, Z=high-impedance
        sc_lv<8> lv2 = "1100XZXZ";
        std::cout << "lv1       = " << lv1 << std::endl;
        std::cout << "lv2       = " << lv2 << std::endl;
        std::cout << "lv1 & lv2 = " << (lv1 & lv2) << "  (X/Z propagate)" << std::endl;

        // sc_int: signed fixed-width
        std::cout << "\n--- sc_int (signed) ---" << std::endl;
        sc_int<8> signed_val = -42;
        std::cout << "signed_val = " << signed_val << " = "
                  << signed_val.to_string(SC_BIN) << " (2's complement)" << std::endl;

        // sc_biguint: >64 bits
        std::cout << "\n--- sc_biguint (wide) ---" << std::endl;
        sc_biguint<128> big = 1;
        big <<= 100;
        std::cout << "2^100 = " << big.to_string(SC_HEX) << std::endl;
    }

    SC_CTOR(BitOps) { SC_THREAD(run); }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 6: SystemC data types ===" << std::endl;
    std::cout << "ALU with sc_uint, bit ops, sc_bv, sc_lv\n" << std::endl;

    sc_signal<sc_uint<8>> a, b, result;
    sc_signal<sc_uint<2>> op;
    sc_signal<bool>       carry;

    ALU alu("alu");
    alu.a(a); alu.b(b); alu.op(op);
    alu.result(result); alu.carry(carry);

    BitOps bitops("bitops");

    // Drive ALU
    std::cout << "--- ALU operations ---" << std::endl;
    a.write(200); b.write(100); op.write(0);  // ADD: 200+100=300 (overflow!)
    sc_start(10, SC_NS);

    a.write(50); b.write(30); op.write(1);    // SUB: 50-30=20
    sc_start(10, SC_NS);

    a.write(0xFF); b.write(0x0F); op.write(2); // AND
    sc_start(10, SC_NS);

    a.write(0xF0); b.write(0x0F); op.write(3); // OR
    sc_start(10, SC_NS);

    return 0;
}
