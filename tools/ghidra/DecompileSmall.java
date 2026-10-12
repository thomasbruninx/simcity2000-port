// Headless: decompile every function with body size <= maxBytes. Args: out.txt maxBytes
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class DecompileSmall extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs(); long max = Long.parseLong(a[1]);
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (f.getBody().getNumAddresses() > max || f.isThunk()) continue;
                DecompileResults r = di.decompileFunction(f, 30, monitor);
                if (!r.decompileCompleted()) continue;
                w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint() + " size " + f.getBody().getNumAddresses());
                w.println(r.getDecompiledFunction().getC());
            }
        }
    }
}
