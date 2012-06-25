import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.text.DecimalFormat;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

public class GetFastaStats {  
   private static final int MIN_GAP_SIZE 			= 20;
   private static int MIN_LENGTH   			= 2000;
   private static final int CONTIG_AT_INITIAL_STEP 	= 1000000;
  
   private static final String[] suffix = {"final", "split", "fasta", "scafSeq", "fa"}; 
   private static final NumberFormat nf = new DecimalFormat("############.##");
      
   private class ContigAt {
	   ContigAt(long currentBases) {
		   this.count = this.len = 0;
		   this.totalBP = 0;
		   this.goal = currentBases;
	   }
	   
	   public int count;
	   public long totalBP;
	   public int len;
	   public long goal;
   }
   
   boolean baylorFormat = false;
   boolean oldStyle = false;
   boolean storeCtgs = false;
   HashMap<String, Integer> fastaLens = new HashMap<String, Integer>();
   HashMap<String, String> ctgs = new HashMap<String, String>();
   int totalCount = 0;
   private long genomeSize = 0;
 
   public GetFastaStats(boolean useBaylor, boolean old, long g) {
	   baylorFormat = useBaylor;
	   oldStyle = old;
           genomeSize = g;
   }
   
   public void processFile(String inputFile) throws Exception {
      if (inputFile.contains("lens")) {
         BufferedReader bf = Utils.getFile(inputFile, "lens");
         String line = null;
         while ((line = bf.readLine()) != null) {
            String[] splitLine = line.trim().split("\\s+");
            fastaLens.put(splitLine[0], Integer.parseInt(splitLine[1]));
         }
         bf.close();
      } else {
         BufferedReader bf = Utils.getFile(inputFile, suffix);
      
      String line = null;
      StringBuffer fastaSeq = new StringBuffer();
      String header = "";
      
      while ((line = bf.readLine()) != null) {
         if (line.startsWith(">")) {
        	 String fastaString = fastaSeq.toString().replaceAll("-", "");
        	 fastaString = fastaString.toString().replaceAll("\\.", "");

        	 //String[] split = fastaString.trim().split("N+");
        	 //String[] split = { fastaString.replaceAll("N", "").trim() };
        	 String[] split = { fastaString.trim() };
//System.err.println("SPLIT one ctg of length " + fastaString.length() + " INTO " + split.length);

        	 for (int i = 0; i < split.length; i++) {
        		 if (split[i].length() != 0) {
        			 fastaLens.put(header + "_" + i,split[i].length());               
        			 if (storeCtgs)
        				 ctgs.put(header + "_" + i, split[i].toString());
        		 }
        	}
            //header = line.replace(">contig_", "").trim();
            header = line.trim().split("\\s+")[0].replace(">", "").replaceAll("ctg", "").trim();
            header = line.trim().split("\\s+")[0].replace(">", "").replaceAll("scf", "").trim();
            header = line.trim().split("\\s+")[0].replace(">", "").replaceAll("deg", "").trim();
            fastaSeq = new StringBuffer();
         }
         else {
            fastaSeq.append(line);
         }
      }

      String fastaString = fastaSeq.toString().replaceAll("-", "");
      fastaString = fastaString.toString().replaceAll("\\.", "");

      //String[] split = fastaString.trim().split("N+");
//System.err.println("SPLIT one ctg of length " + fastaString.length() + " INTO " + split.length);
      //String[] split = { fastaString.replaceAll("N", "").trim() };
      String[] split = { fastaString.trim() };
      for (int i = 0; i < split.length; i++) {
    	  if (fastaSeq.length() != 0) {
    		  fastaLens.put(header + "_" + i,split[i].length());
         
    		  if (storeCtgs)
    			  ctgs.put(header + "_" + i, split[i].toString());
    	  }
      }
      bf.close();
      }
   }
   
   public void countReads(String posmapFile, boolean isNB) throws Exception {
	      BufferedReader bf = new BufferedReader(new InputStreamReader(
	              new FileInputStream(posmapFile)));
       
       String line = null;
       String currID = "";
       int readCount = 0;       
       totalCount = 0;
       int contigIDIndex = (isNB ? 2 : 1);
       
       while ((line = bf.readLine()) != null) {
    	   String[] splitLine = line.trim().split("\\s+");
    	   
    	   if (splitLine.length <= contigIDIndex) {
    		   continue;
    	   }
    	   String id = splitLine[contigIDIndex].trim();
    	       	   
    	   if (!id.equalsIgnoreCase(currID)) {
    		   totalCount += readCount;
    		   currID = id;
    		   readCount = 0;
    	   }
    	   if (fastaLens.get(id) != null) {
    		   readCount++;
    	   }
       }
       if (readCount != 0) {
    	   totalCount += readCount;
       }
   }

   public void convertToScaffolds(String scaffFile) throws Exception {
      BufferedReader bf = new BufferedReader(new InputStreamReader(
            new FileInputStream(scaffFile)));
      
      String line = null;
      String currID = "";
      StringBuffer fastaSeq = new StringBuffer();
      int negGaps = 0;
      String id = "";
      
      while ((line = bf.readLine()) != null) {
         String[] splitLine = line.trim().split("\\s+");         
         
         try {
            if (splitLine[0].equalsIgnoreCase("supercontig")) {
               id = splitLine[1];
            }
            if (splitLine[0].equalsIgnoreCase("contig")) {
               fastaSeq.append(ctgs.get(splitLine[1]));
            }
            if (splitLine[0].equalsIgnoreCase("gap")) {
               int gapSize = Integer.parseInt(splitLine[1]);
               
               if (gapSize < 0) {
                  negGaps += gapSize;
                  fastaSeq.delete(fastaSeq.length()+gapSize, fastaSeq.length());
                  //gapSize = MIN_GAP_SIZE;
               }
               for (int i = 0; i < gapSize; i++) {
                  fastaSeq.append("N");
               }               
            }
         } catch (Exception e) {
            continue;
         }
         
         if (!currID.equalsIgnoreCase(id)) {
            if (fastaSeq.length() != 0) {
               System.err.println(">scaffold_" + currID);
               System.err.println(Utils.convertToFasta(fastaSeq.toString().trim()));               
               fastaSeq = new StringBuffer();
            }
            currID = id;
         }
      }

      if (fastaSeq.length() != 0) {
         System.err.println(">scaffold_" + currID);
         System.err.println(Utils.convertToFasta(fastaSeq.toString().trim()));
      }
      
      System.out.println("NEGATIVE GAPS IS " + negGaps);
   }

   public String toString(boolean outputHeader, String title) {
      StringBuffer st = new StringBuffer();
      int max = Integer.MIN_VALUE;
      int min = Integer.MAX_VALUE;
      long total = 0;
      int count = 0;
      
      int n10 = 0;
      int n25 = 0;
      int n50 = 0;
      int n75 = 0;
      int n95 = 0;
      
      int n10count = 0;
      int n25count = 0;
      int n50count = 0;
      int n75count = 0;
      int n95count = 0;

      int totalOverLength = 0;
      long totalBPOverLength = 0;

      double fSize = 0;
      for (String s : fastaLens.keySet()) {
         int len = fastaLens.get(s);
        
if (len <= MIN_LENGTH) {
continue;
} 
         if (oldStyle == true && len <= MIN_LENGTH) {
        	 continue;
         }
         
         if (len > max) { max = len; }
         if (len < min) { min = len; }
         if (genomeSize == 0) { 
            total += len;
         } else {
            total = genomeSize;
         }  
         count++;
        
         // compute the F-size
         fSize += Math.pow(len, 2); 
         if (len > MIN_LENGTH) {
        	 totalOverLength++;
        	 totalBPOverLength += len;
         }
      }
      fSize /= genomeSize;
 
      // get the goal contig at X bases (1MBp, 2MBp)
      ArrayList<ContigAt> contigAtArray = new ArrayList<ContigAt>();
      if (baylorFormat == true) {
    	  contigAtArray.add(new ContigAt( 1 * CONTIG_AT_INITIAL_STEP));
    	  contigAtArray.add(new ContigAt( 2 * CONTIG_AT_INITIAL_STEP));
    	  contigAtArray.add(new ContigAt( 5 * CONTIG_AT_INITIAL_STEP));
    	  contigAtArray.add(new ContigAt(10 * CONTIG_AT_INITIAL_STEP));
      } 
      else {      
	      long step = CONTIG_AT_INITIAL_STEP;
	      long currentBases = 0;
	      while (currentBases <= total) {
	    	  if ((currentBases / step) >= 10) {
	    		  step *= 10;
	    	  }
	    	  currentBases += step;
	    	  contigAtArray.add(new ContigAt(currentBases));
	      }
      }
      ContigAt[] contigAtVals = contigAtArray.toArray(new ContigAt[0]);

      Integer[] vals = fastaLens.values().toArray(new Integer[0]);
      Arrays.sort(vals);

      long sum = 0;
      double median = 0;
      int medianCount = 0;
      int numberContigsSeen = 1;
      int currentValPoint = 0;
      for (int i = vals.length - 1; i >= 0; i--) {
    	  if (((int) (count / 2)) == i) {
    		  median += vals[i];
    		  medianCount++;
    	  }
    	  else if (count % 2 == 0 && ((((int) (count / 2)) + 1) == i)) {
    		  median += vals[i];
    		  medianCount++;
    	  }
    	  
         sum += vals[i];

         // calculate the bases at
         while (currentValPoint < contigAtVals.length && sum >= contigAtVals[currentValPoint].goal && contigAtVals[currentValPoint].count == 0) {
System.err.println("Calculating point at " + currentValPoint + " and the sum is " + sum + " and i is" + i + " and lens is " + vals.length + " and length is " + vals[i]);
        	 contigAtVals[currentValPoint].count = numberContigsSeen;
        	 contigAtVals[currentValPoint].len = vals[i];
        	 contigAtVals[currentValPoint].totalBP = sum;
        	 currentValPoint++;
         }
         // calculate the NXs
         if (sum / (double)total >= 0.1 && n10count == 0) {
        	 n10 = vals[i];
        	 n10count = vals.length - i;
        	 
         }
         if (sum / (double)total >= 0.25 && n25count == 0) {
        	 n25 = vals[i];
        	 n25count = vals.length - i;
        	 
         }
         if (sum / (double)total >= 0.5 && n50count == 0) {
        	 n50 = vals[i];
        	 n50count = vals.length - i;
        	 
         }
         if (sum / (double)total >= 0.75 && n75count == 0) {
                n75 = vals[i];
                n75count = vals.length - i;
         }
         if (sum / (double)total >= 0.95 && n95count == 0) {
        	 n95 = vals[i];
        	 n95count = vals.length - i;
        	 
         }
         
         numberContigsSeen++;
      }
      if (medianCount != 1 && medianCount != 2) {
    	  System.err.println("ERROR INVALID MEDIAN COUNT " + medianCount);
    	  System.exit(1);
      }
      
      if (oldStyle == true) {
          st.append("Total units: " + count + "\n");
          st.append("Reference: " + total + "\n");
          st.append("BasesInFasta: " + totalBPOverLength + "\n");
          st.append("Min: " + min + "\n");
          st.append("Max: " + max + "\n");
          st.append("N10: " + n10 + " COUNT: " + n10count + "\n");
          st.append("N25: " + n25 + " COUNT: " + n25count + "\n");
          st.append("N50: " + n50 + " COUNT: " + n50count + "\n");
          st.append("N75: " + n75 + " COUNT: " + n75count + "\n");
          st.append("E-size:" + nf.format(fSize) + "\n");
      } else {
	      if (outputHeader) {
	    	  st.append("Assembly");
	    	  st.append(",Unit Number");
		      st.append(",Unit Total BP");
		      st.append(",Number Units > " + MIN_LENGTH);
		      st.append(",Total BP in Units > " + MIN_LENGTH);
		      st.append(",Min");
		      st.append(",Max");
		      st.append(",Average");
		      st.append(",Median");
		      
		      for (int i = 0; i < contigAtVals.length; i++) {
		    	  if (contigAtVals[i].count != 0) {
		    		  st.append(",Unit At " + nf.format(contigAtVals[i].goal) + " Unit Count," + /*"Total Length," + */ " Actual Unit Length" );
		    	  }
		      }
		      st.append("\n");
	      }
	      
	      st.append(title);
	      st.append("," + nf.format(count));
	      st.append("," + nf.format(total));
	      st.append("," + nf.format(totalOverLength));
	      st.append("," + nf.format(totalBPOverLength));
	      st.append("," + nf.format(min));
	      st.append("," + nf.format(max));
	      st.append("," + nf.format((double)total / count));
	      st.append("," + nf.format((double)median / medianCount));
	      
	      for (int i = 0; i < contigAtVals.length; i++) {
	    	  if (contigAtVals[i].count != 0) {
	    		  st.append("," + contigAtVals[i].count + "," + /*contigAtVals[i].totalBP + "," +*/ contigAtVals[i].len);
	    	  }
	      }
      }
      
      return st.toString();
   }
   
   public static void printUsage() {
      System.err.println("This program computes total bases in fasta sequences, N50, min, and max.");

   }
   public static void main(String[] args) throws Exception {     
      if (args.length < 1) { printUsage(); System.exit(1);}

      /*
      if (args.length > 1) {
         f.storeCtgs = true;
      }
      */           
      
      boolean useBaylorFormat = false;
      boolean oldStyle = false;
      long genomeSize = 0;
      int initialVal = 0;
      while (args[initialVal].startsWith("-")) {
    	  if (args[initialVal].trim().equalsIgnoreCase("-b")) {
    		  useBaylorFormat = true;
    	  }
          else if (args[initialVal].trim().equalsIgnoreCase("-min")) {
                  GetFastaStats.MIN_LENGTH = Integer.parseInt(args[++initialVal]);
          }
    	  else if (args[initialVal].trim().equalsIgnoreCase("-o")) {
    		  oldStyle = true;
    	  }
          else if (args[initialVal].trim().equalsIgnoreCase("-genomeSize")) {
                  initialVal++;
                  genomeSize = Long.parseLong(args[initialVal]);
System.err.println("Found genome size at position " + initialVal + " with value " + args[initialVal] + " aka " + genomeSize);
          }
    	  else {
    		  System.err.println("Unknown parameter "+ args[initialVal] + " specified, please specify -b for Baylor-style output.");
    		  System.exit(1);
    	  }
    	  initialVal++;
      }
      
      for (int i = initialVal; i < args.length; i++) {
    	  String assemblyTitle = args[i].trim().split("/")[0];
          GetFastaStats f = new GetFastaStats(useBaylorFormat, oldStyle, genomeSize);
          String[] splitLine = args[i].trim().split(",");

          for (int j = 0; j < splitLine.length; j++) {
        	  f.processFile(splitLine[j]);
    	  }
          System.out.println(f.toString(i == initialVal, assemblyTitle));
      }            
      /*
      if (args.length > 1) {
         //f.convertToScaffolds(args[1]);
    	  f.countReads(args[1], Boolean.parseBoolean(args[2]));
      }
      */           
   }
}
