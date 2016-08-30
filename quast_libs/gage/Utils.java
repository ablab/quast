//***************************************************************************
//* Copyright (c) 2011 Steven L. Salzberg et al.
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;

import java.io.File;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileReader;

public class Utils {
   public static final int MBYTES = 1048576;
   public static final int FASTA_LINE_LENGTH = 60;
   public static MemoryMXBean mbean = ManagementFactory.getMemoryMXBean();
   
   public static class Pair {
      public int first;
      public double second;
      public String identifier;
      
      public Pair(int first, double second) {
         this.first = first;
         this.second = second;
      }

      public Pair(int first, double second, String third) {
         this.first = first;
         this.second = second;
         this.identifier = third;
      }
      
      public int size() {
         return (Math.max(first, (int)second) - Math.min(first, (int)second) + 1);
      }
   }
   
   public enum Translate
   {
       A("T"),
       C("G"),
       G("C"),
       T("A"),
       N("N");

       private String other;

       public String getCompliment()
       {
          return other;
       }
       Translate( String other )
       {
          this.other = other;
       }
   }
   
   public enum ToProtein
   {
      GCT("A"),
      GCC("A"),
      GCA("A"),
      GCG("A"),
      TTA("L"),
      TTG("L"),
      CTT("L"),
      CTC("L"),
      CTA("L"),
      CTG("L"),      
      CGT("R"),
      CGC("R"),
      CGA("R"),
      CGG("R"),
      AGA("R"),
      AGG("R"),
      AAA("K"),
      AAG("K"),
      AAT("N"),
      AAC("N"),
      ATG("M"),
      GAT("D"),
      GAC("D"),
      TTT("F"),
      TTC("F"),
      TGT("C"),
      TGC("C"),
      CCT("P"),
      CCC("P"),
      CCA("P"),
      CCG("P"),
      CAA("Q"),
      CAG("Q"),
      TCT("S"),
      TCC("S"),
      TCA("S"),
      TCG("S"),
      AGT("S"),
      AGC("S"),
      GAA("E"),
      GAG("E"),
      ACT("T"),
      ACC("T"),
      ACA("T"),
      ACG("T"),
      GGT("G"),
      GGC("G"),
      GGA("G"),
      GGG("G"),
      TGG("W"),
      CAT("H"),
      CAC("H"),
      TAT("Y"),
      TAC("Y"),
      ATT("I"),
      ATC("I"),
      ATA("I"),
      GTT("V"),
      GTC("V"),
      GTA("V"),
      GTG("V"),
      TAG("X"),
      TGA("X"),
      TAA("X");
      
      /*
      Ala/A    GCU, GCC, GCA, GCG   
      Leu/L    UUA, UUG, CUU, CUC, CUA, CUG
      Arg/R    CGU, CGC, CGA, CGG, AGA, AGG  
      Lys/K    AAA, AAG
      Asn/N    AAU, AAC    
      Met/M    AUG
      Asp/D    GAU, GAC    
      Phe/F    UUU, UUC
      Cys/C    UGU, UGC    
      Pro/P    CCU, CCC, CCA, CCG
      Gln/Q    CAA, CAG    
      Ser/S    UCU, UCC, UCA, UCG, AGU, AGC
      Glu/E    GAA, GAG    
      Thr/T    ACU, ACC, ACA, ACG
      Gly/G    GGU, GGC, GGA, GGG   
      Trp/W    UGG
      His/H    CAU, CAC    
      Tyr/Y    UAU, UAC
      Ile/I    AUU, AUC, AUA  
      Val/V    GUU, GUC, GUA, GUG
      START    AUG   
      STOP  UAG, UGA, UAA
      */
       private String other;

       public String getProtein()
       {
          return other;
       }
       ToProtein( String other )
       {
          this.other = other;
       }
   }


   public static BufferedReader getFile(String fileName, String postfix) throws Exception {
      String[] array = new String[1];
      array[0] = postfix;

      return getFile(fileName, array);
   }

   public static BufferedReader getFile(String fileName, String[] postfix) throws Exception {
       BufferedReader bf = null;

       if (fileName.endsWith("bz2")) {
          // open file as a pipe
          System.err.println("Running command " + "bzip2 -dc " + new File(fileName).getAbsolutePath() + " |");
          Process p = Runtime.getRuntime().exec("bzip2 -dc " + new File(fileName).getAbsolutePath() + " |");
          bf = new BufferedReader(new InputStreamReader(p.getInputStream()));
          System.err.println(bf.ready());
        } else if (fileName.endsWith("gz")) {
          // open file as a pipe
           System.err.println("Runnning comand " + "gzip -dc " + new File(fileName).getAbsolutePath() + " |");
           Process p = Runtime.getRuntime().exec("gzip -dc " + new File(fileName).getAbsolutePath() + " |");
           bf = new BufferedReader(new InputStreamReader(p.getInputStream()));
           System.err.println(bf.ready());
        } else {
           int i = 0;
           for (i = 0; i < postfix.length; i++) {
              if (fileName.endsWith(postfix[i])){
                 bf = new BufferedReader(new FileReader(fileName));
                 break;
              }
           }
           if (i == postfix.length) {
              // System.err.println("Unknown file format " + fileName + " Skipping!");  
              bf = new BufferedReader(new FileReader(fileName)); // we can specify FASTA-file without extension
           }
        }

        return bf;
   }

   // add new line breaks every FASTA_LINE_LENGTH characters
   public static String convertToFasta(String supplied) {      
      StringBuffer converted = new StringBuffer();      
      int i = 0;
      
      for (i = 0; (i+FASTA_LINE_LENGTH) < supplied.length(); i+= FASTA_LINE_LENGTH) {
         converted.append(supplied.substring(i, i+FASTA_LINE_LENGTH));
         converted.append("\n");         
      }
      converted.append(supplied.substring(i, supplied.length()));
      
      return converted.toString();
   }
   
   public static String rc(String supplied) {
      StringBuilder st = new StringBuilder();
      for (int i = supplied.length() - 1; i >= 0; i--) {
         char theChar = supplied.charAt(i);         
         
         if (theChar != '-') {
            Translate t = Translate.valueOf(Character.toString(theChar).toUpperCase());
            st.append(t.getCompliment());
         } else {
            st.append("-");
         }
      }
      return st.toString();
   }

   public static String getUngappedRead(String fasta) {
      fasta = fasta.replaceAll("N", "");
      fasta = fasta.replaceAll("-", "");
      
      assert(fasta.length() >= 0);
      
      return fasta;
   }

   public static int countLetterInRead(String fasta, String letter) {
      return countLetterInRead(fasta, letter, false);
   }

   public static int countLetterInRead(String fasta, String letter, Boolean caseSensitive) {
      String ungapped = Utils.getUngappedRead(fasta);
      int len = ungapped.length();
      if (len == 0) { return -1; }
   
      int increment = letter.length();
      int count = 0;
      
      for (int i = 0; i <= ungapped.length() - increment; i += increment) {
         if (letter.equals(ungapped.substring(i, i+increment)) && caseSensitive) {
            count++;
         }
         if (letter.equalsIgnoreCase(ungapped.substring(i, i+increment)) && !caseSensitive) {
            count++;
         }
      }
      return count;
   }
   
   public static double getLetterPercentInRead(String fasta, String letter) {
      int ungappedLen = getUngappedRead(fasta).length();
      int count = countLetterInRead(fasta, letter);
      
      return count / (double)ungappedLen;
   }

   public static String toProtein(String genome, boolean isReversed, int frame) {
      StringBuilder result = new StringBuilder();

      if (isReversed) {
         genome = rc(genome);
      }
      genome = genome.replaceAll("-", "");
      
      for (int i = frame; i < (genome.length() - 3); i += 3) {
         String codon = genome.substring(i, i+3);
         String protein = ToProtein.valueOf(codon).getProtein();
         result.append(protein);
      }
      
      return result.toString();
   }
   
   public static int checkForEnd(String line, int brackets) {
      if (line.startsWith("{")) {
         brackets++;
      }
      if (line.startsWith("}")) {
         brackets--;
      }
      if (brackets == 0) {
         return -1;
      }
      
      return brackets;
   }
   
   public static String getID(String line) {
      String ids[] = line.split(":");
      int commaPos = ids[1].indexOf(",");
      if (commaPos != -1) {      
         return ids[1].substring(1, commaPos).trim();
      } else {
         return ids[1];
      }
   }
   
   public static String getValue(String line, String key) {
      if (line.startsWith(key)) {
         return line.split(":")[1];
      }

      return null;
   }
   
   public static int getOvlSize(int readA, int readB, int ahang, int bhang) {
      if ((ahang <= 0 && bhang >= 0) || (ahang >= 0 && bhang <= 0)) {
         return -1;
      }
      
      if (ahang < 0) {
         return readA - Math.abs(bhang);
      }
      else {
         return readA - ahang;
      }
   }
   
   public static int getRangeOverlap(int startA, int endA, int startB, int endB) {
      int minA = Math.min(startA, endA);
      int minB = Math.min(startB, endB);
      int maxA = Math.max(startA, endA);
      int maxB = Math.max(startB, endB);
      
      int start = Math.max(minA, minB);
      int end = Math.min(maxA, maxB);
      
      return (end-start+1);
   }
   
   public static boolean isAContainedInB(int startA, int endA, int startB, int endB) {
      int minA = Math.min(startA, endA);
      int minB = Math.min(startB, endB);
      int maxA = Math.max(startA, endA);
      int maxB = Math.max(startB, endB);

      return (minB < minA && maxB > maxA);
   }
}
