// Ghidra headless script: dumps "VA,size,name" for every function to a CSV (argument 0 = output path).
// Run: analyzeHeadless <proj_dir> <proj> -process SIMCITY.EXE -noanalysis -scriptPath tools/ghidra -postScript ExportFunctions.java out.csv
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs().length > 0 ? getScriptArgs()[0] : "functions.csv";
        int n = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.println("va,size,name");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                w.printf("0x%08x,%d,%s%n", f.getEntryPoint().getOffset(), f.getBody().getNumAddresses(), f.getName());
                n++;
            }
        }
        println("EXPORTED_FUNCTIONS=" + n);
    }
}
