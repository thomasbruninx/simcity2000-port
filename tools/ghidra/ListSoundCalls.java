// Headless: every call to the sound routines 0x424AA0 (play), 0x424AD0 (play with repeat), 0x424AF0 (stop) with the pushed immediate. Args: out.txt
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.Scalar;
import java.io.*;
import java.util.*;

public class ListSoundCalls extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Set<Long> targets = new HashSet<>(Arrays.asList(0x424aa0L, 0x424ad0L, 0x424af0L, 0x424b10L));
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            InstructionIterator it = currentProgram.getListing().getInstructions(true);
            List<Instruction> window = new ArrayList<>();
            while (it.hasNext()) {
                Instruction in = it.next();
                window.add(in); if (window.size() > 8) window.remove(0);
                if (!in.getFlowType().isCall()) continue;
                for (Reference r : in.getReferencesFrom()) {
                    Function c = getFunctionAt(r.getToAddress());
                    if (c == null) continue;
                    if (c.isThunk()) c = c.getThunkedFunction(true);
                    long ep = c.getEntryPoint().getOffset();
                    if (!targets.contains(ep)) continue;
                    String imm = "?"; int pushes = 0; String all = "";
                    for (int i = window.size() - 2; i >= 0 && pushes < 4; i--) {
                        Instruction p = window.get(i);
                        if (p.getMnemonicString().equals("PUSH")) {
                            pushes++;
                            Object[] objs = p.getOpObjects(0);
                            all += (objs.length > 0 && objs[0] instanceof Scalar ? Long.toHexString(((Scalar) objs[0]).getUnsignedValue()) : p.getDefaultOperandRepresentation(0)) + " ";
                        }
                    }
                    Function f = getFunctionContaining(in.getAddress());
                    w.println(Long.toHexString(ep) + " at " + in.getAddress() + " in " + (f == null ? "?" : f.getName()) + " pushes: " + all);
                }
            }
        }
    }
}
