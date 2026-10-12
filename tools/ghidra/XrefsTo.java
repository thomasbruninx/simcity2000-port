// Headless: list references to addresses/strings. Args: out.txt addr [addr...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import java.io.*;

public class XrefsTo extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (int i = 1; i < a.length; i++) {
                Address ad = toAddr(Long.parseLong(a[i].replace("0x", ""), 16));
                w.println("== " + a[i]);
                for (Reference r : getReferencesTo(ad)) {
                    ghidra.program.model.listing.Function f = getFunctionContaining(r.getFromAddress());
                    w.println("  from " + r.getFromAddress() + " " + r.getReferenceType() + " in " + (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
                }
            }
        }
    }
}
