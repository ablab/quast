import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.text.DecimalFormat;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

public class SizeFasta {  
  private static final NumberFormat nf = new DecimalFormat("############.#");
   private boolean ungapped = false;
   private static final String[] suffix = {"contig", "scafSeq", "fa", "scafSeq", "fna", "fasta", "final"}; 
   private static final String[] suffixFQ = {"fastq", "fq", "txt"};

   public SizeFasta() {
   }

   private int getFastaStringLength(StringBuffer fastaSeq) {
      return (ungapped == false ? fastaSeq.length() : fastaSeq.toString().replaceAll("N", "").replaceAll("n", "").replaceAll("-", "").length());
   }

   public void processFasta(String inputFile) throws Exception {
      BufferedReader bf = Utils.getFile(inputFile, suffix);
      
      String line = null;
      StringBuffer fastaSeq = new StringBuffer();
      String header = "";
      
      while ((line = bf.readLine()) != null) {
         if (line.startsWith(">")) {
            if (fastaSeq.length() != 0) {System.out.println(header + "\t" + getFastaStringLength(fastaSeq)); }
            header = line.substring(1);
            fastaSeq = new StringBuffer();
         }
         else {
            fastaSeq.append(line);
         }
      }

      if (fastaSeq.length() != 0) { System.out.println(header + "\t" + getFastaStringLength(fastaSeq)); }
      bf.close();
   }

   public void processFastq(String inputFile) throws Exception {
      BufferedReader bf = Utils.getFile(inputFile, suffixFQ);

      String line = null;
      StringBuffer fastaSeq = new StringBuffer();
      String header = "";

      while ((line = bf.readLine()) != null) {
         // read four lines at a time for fasta, qual, and headers
         String ID = line.split("\\s+")[0].substring(1);
         String fasta = bf.readLine();
         String qualID = bf.readLine().split("\\s+")[0].substring(1);

         if (qualID.length() != 0 && !qualID.equals(ID)) {
            System.err.println("Error ID " + ID + " DOES not match quality ID " + qualID);
            System.exit(1);
         }
         String qualSeq = bf.readLine();
         System.out.println(header + "\t" + fasta.length());
      }

      bf.close();
   }

   public static void printUsage() {
      System.err.println("This program sizes a fasta or fastq file. Multiple fasta files can be supplied by using a comma-separated list.");
      System.err.println("Example usage: SizeFasta fasta1.fasta,fasta2.fasta");
   }
   
   public static void main(String[] args) throws Exception {     
      if (args.length < 1) { printUsage(); System.exit(1);}

      SizeFasta f = new SizeFasta();
      if (args.length >= 2) { f.ungapped = Boolean.parseBoolean(args[1]); }

      boolean processed = false; 
      String[] splitLine = args; //args[0].trim().split(",");
      for (int j = 0; j < splitLine.length; j++) {
          for (String s : SizeFasta.suffixFQ) {
             if (splitLine[j].contains(s)) {
                f.processFastq(splitLine[j]);
                processed = true;
                break;
             }
          }
          if (!processed) {
     	  for (String s : SizeFasta.suffix) {
             if (splitLine[j].contains(s)) {
                f.processFasta(splitLine[j]);
                processed = true;
                break;
             }
           }
          } else {
             System.err.println("Unknown file type " + splitLine[j]);
          }
       }
   }
}
