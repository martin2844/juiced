import org.openjuice.android.GameDataImport;
import java.io.*;
import java.nio.file.*;
import java.util.zip.*;
public class GameDataImportTest {
    static byte[] zip(String... names) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream z = new ZipOutputStream(bytes)) {
            for (String name : names) {
                z.putNextEntry(new ZipEntry(name));
                if (!name.endsWith("/")) z.write(new byte[]{1,2,3});
                z.closeEntry();
            }
        }
        return bytes.toByteArray();
    }
    static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static void run(File root, byte[] zip) throws IOException {
        GameDataImport.extract(new ByteArrayInputStream(zip),root,(files,bytes)->{});
    }
    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("juiced-import-test-").toFile();
        run(root,zip("Juiced/tracks/", "Juiced/carmodels.dat", "Juiced/scripts.dat", "Juiced/Juiced.exe"));
        check(GameDataImport.valid(new File(root,"game")),"wrapped ZIP import");
        check(!new File(root,"game/Juiced.exe").exists(),"Windows binaries excluded");
        try { run(root,zip("../escape")); throw new AssertionError("traversal accepted"); }
        catch (IOException expected) { }
        check(GameDataImport.valid(new File(root,"game")),"failed import preserved prior data");
        check(!new File(root,"escape").exists(),"no path escaped staging");
        try { run(root,zip("unrelated.txt")); throw new AssertionError("invalid game accepted"); }
        catch (IOException expected) { }
        check(GameDataImport.valid(new File(root,"game")),"invalid archive preserved prior data");
        run(root,zip("tracks/", "carmodels.dat", "scripts.dat"));
        check(GameDataImport.valid(new File(root,"game")),"flat ZIP replacement");
        check(!new File(root,"game-previous").exists(),"old data cleaned after successful swap");
        check(new File(root,"game").renameTo(new File(root,"game-previous")),"simulate interrupted swap");
        try { run(root,zip("bad.txt")); } catch(IOException expected) { }
        check(GameDataImport.valid(new File(root,"game")),"interrupted swap recovered");
        run(root,zip("Juiced/TRACKS/", "Juiced/CarModels.dat", "Juiced/Scripts.DAT"));
        check(GameDataImport.valid(new File(root,"game")),"mixed-case retail names");
        try (var paths = Files.walk(root.toPath())) {
            paths.sorted(java.util.Comparator.reverseOrder()).forEach(p -> {try {Files.delete(p);} catch(IOException e) {throw new RuntimeException(e);} });
        }
        System.out.println("ZIP importer: valid roots, filtering, traversal rejection, preservation, replacement and recovery passed");
    }
}
