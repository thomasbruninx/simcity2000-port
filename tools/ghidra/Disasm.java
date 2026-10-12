// Headless: disassemble a VA range. Args: out.txt startVA endVA   (forces disassembly if needed)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;

public class Disasm extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address s = toAddr(Long.parseLong(a[1].replace("0x",""),16)), e = toAddr(Long.parseLong(a[2].replace("0x",""),16));
        disassemble(s);
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            Instruction in = getInstructionAt(s);
            if (in == null) in = getInstructionAfter(s);
            while (in != null && in.getAddress().compareTo(e) <= 0) {
                w.println(in.getAddress() + "  " + in.toString());
                in = in.getNext();
            }
        }
    }
}
