This small tutorial shows how to add a new metric to a `report.txt`
1. Create your method in the file `quast_libs`. Notice that it should be placed reasonably. 
But feel free to add a new .py file 
2. Create a field in the `reporting.py` - a variable in the class `Field`. 
The value of this variable (string) will be printed in the report as a metric name. 
Also, do not forget to add the field in the 
corresponding groups, for instance `Genome statistics` or `Misassemblies`. 
3. Add your metric to the printing methods - `aligned_stats.py` for 
the metrics after the contigs were aligned or `basic_stats.py` otherwise. Call your metric method and 
put it value to the report like this `report.add_field(reporting.Fields.*field_name*, *metric value*)`.
If you what to log your metric - add it to the `logger.info` manually.
4. Run  ``./quast.py test_data/contigs_1.fasta        
               test_data/contigs_2.fasta     
               -r test_data/reference.fasta.gz   
               -g test_data/genes.gff`` to test. (details are [here](http://quast.sourceforge.net/docs/manual.html)). 
This will generate the `quast_results` folder. 
Inside there will folders with the time of run. Go further inside and check `report.txt`.
