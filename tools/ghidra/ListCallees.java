// Headless: for the functions containing the given addresses list every callee (thunks resolved) with its RET immediate (callee-cleaned bytes).
// Args: out.txt addr [addr...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ListCallees extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        TreeMap<String, String> out = new TreeMap<>();
        for (int i = 1; i < a.length; i++) {
            Function f = getFunctionContaining(toAddr(Long.parseLong(a[i].replace("0x", ""), 16)));
            if (f == null) continue;
            InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
            while (it.hasNext()) {
                Instruction in = it.next();
                if (!in.getFlowType().isCall()) continue;
                for (Reference r : in.getReferencesFrom()) {
                    Function c = getFunctionAt(r.getToAddress());
                    if (c == null) { out.put(r.getToAddress().toString(), "(no function, " + in.toString() + ") in " + f.getName()); continue; }
                    if (c.isThunk()) c = c.getThunkedFunction(true);
                    int ret = -1;
                    InstructionIterator ci = currentProgram.getListing().getInstructions(c.getBody(), true);
                    String retTxt = "";
                    while (ci.hasNext()) { Instruction x = ci.next(); if (x.getMnemonicString().equals("RET")) { retTxt = x.toString(); } }
                    out.put(c.getEntryPoint().toString(), c.getName() + " size " + c.getBody().getNumAddresses() + " last " + retTxt + " (from " + f.getName() + ")");
                }
            }
        }
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) { for (var e : out.entrySet()) w.println(e.getKey() + "  " + e.getValue()); }
    }
}
