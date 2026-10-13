// Headless: list instructions with an immediate operand in [lo, hi] (decimal or 0x). Args: out.txt lo hi
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.io.*;

public class ScanScalars extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long lo = Long.decode(a[1]), hi = Long.decode(a[2]);
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            InstructionIterator it = currentProgram.getListing().getInstructions(true);
            while (it.hasNext()) {
                Instruction in = it.next();
                for (int i = 0; i < in.getNumOperands(); i++) {
                    Object[] objs = in.getOpObjects(i);
                    for (Object o : objs) {
                        if (o instanceof Scalar) {
                            long v = ((Scalar) o).getUnsignedValue();
                            if (v >= lo && v <= hi) {
                                Function f = getFunctionContaining(in.getAddress());
                                w.println(in.getAddress() + "  " + in + "   in " + (f == null ? "?" : f.getName()));
                            }
                        }
                    }
                }
            }
        }
    }
}
