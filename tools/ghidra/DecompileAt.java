// Headless: decompile functions containing given addresses. Args: out.txt addr [addr...]  (hex VA)
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;

public class DecompileAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (int i = 1; i < a.length; i++) {
                Address ad = toAddr(Long.parseLong(a[i].replace("0x", ""), 16));
                Function f = getFunctionContaining(ad);
                if (f == null) { w.println("// no function at " + a[i]); continue; }
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint() + " (requested " + a[i] + ")");
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed");
            }
        }
    }
}
