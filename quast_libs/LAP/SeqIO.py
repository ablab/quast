'''
 ParseFasta - class for reading a fasta-like formatted file.
 
 The assumption is that the file contains a set of multi-line records
 separated by single-line headers starting with a specific separator 
 (by default >)
  
 - Taken from ParseFasta.pm - Mihai Pop
 
 @author Christopher Hill
'''
import time,sys
from UserString import MutableString

class ParseFasta:
    ''' Constructor '''
    def __init__(self, file, head=">", linesep=""):
        # Head/Record separator, default is ">"
        self.headSep = head
        
        # Line seperator used when concatenating the lines in the input
        # forming the body of each record.  Useful for .qual files
        self.lineSep = linesep
        
        # String buffer
        self.buffer = None
        
        #Represents the line buffer
        self.file = open(file)
        
        # If the file doesn't start with a header, exit
        self.buffer = self.file.readline()
        if self.buffer == None or not self.buffer.startswith(self.headSep):
            return None
    
    '''Return head separator'''        
    def getHeadSep(self):
        return self.headSep
    
    ''' Return line separator '''
    def getLineSep(self):
        return self.lineSep
    
    '''            
    Reads a record and returns the head and data in a String array.
     array[0] = head
     array[1] = data
    '''
    def getRecord(self):
        # Stores the head entry
        head = ""
        
        # Stores the data
        data = ""
        
        # If the buffer doesn't start with a header
        if not self.buffer or not self.buffer.startswith(self.headSep):
            return None
            
        # Set the header
        head = self.buffer[len(self.headSep):]
        head = head.rstrip()
        self.buffer = self.file.readline();
        
        # Set the data by continously looping through the record until a new record
        # is reached or the end of file is reached
        while self.buffer and not self.buffer.startswith(self.headSep):
            data += (self.buffer.rstrip() + self.lineSep ) # Might have to add trim
            self.buffer = self.file.readline()
            
        # Prepare the record
        results = [head, data]
        
        return results;

    ''' Close Stream '''
    def closeStream(self):
        self.file.close()


# Modified from: http://scipher.wordpress.com/2010/05/06/simple-python-fastq-parser/
class ParseFastQ(object):
    """Returns a read-by-read fastQ parser analogous to file.readline()"""
    def __init__(self,filePath,headerSymbols=['@','+']):
        """Returns a read-by-read fastQ parser analogous to file.readline().
        Exmpl: parser.next()
        -OR-
        Its an iterator so you can do:
        for rec in parser:
            ... do something with rec ...

        rec is tuple: (seqHeader,seqStr,qualHeader,qualStr)
        """
        self._file = open(filePath, 'rU')
        self._currentLineNumber = 0
        self._hdSyms = headerSymbols
        
    def __iter__(self):
        return self
    
    def next(self):
        """Reads in next element, parses, and does minimal verification.
        Returns: tuple: (seqHeader,seqStr,qualHeader,qualStr)"""
        # ++++ Get Next Four Lines ++++
        elemList = []
        for i in range(4):
            line = self._file.readline()
            self._currentLineNumber += 1 ## increment file position
            if line:
                elemList.append(line.strip('\n'))
            else: 
                elemList.append(None)
        
        # ++++ Check Lines For Expected Form ++++
        trues = [bool(x) for x in elemList].count(True)
        nones = elemList.count(None)
        # -- Check for acceptable end of file --
        if nones == 4:
            raise StopIteration
        # -- Make sure we got 4 full lines of data --
        assert trues == 4,\
               "** ERROR: It looks like I encountered a premature EOF or empty line.\n\
               Please check FastQ file near line #%s (plus or minus ~4 lines) and try again**" % (self._currentLineNumber)
        # -- Make sure we are in the correct "register" --
        assert elemList[0].startswith(self._hdSyms[0]),\
               "** ERROR: The 1st line in fastq element does not start with '%s'.\n\
               Please check FastQ file and try again **" % (self._hdSyms[0])
        assert elemList[2].startswith(self._hdSyms[1]),\
               "** ERROR: The 3rd line in fastq element does not start with '%s'.\n\
               Please check FastQ file and try again **" % (self._hdSyms[1])
        
        # ++++ Return fatsQ data as tuple ++++
        return tuple(elemList)
    
    def getNextReadSeq(self):
        """Convenience method: calls self.getNext and returns only the readSeq."""
        try:
            record = self.next()
            return record[1]
        except StopIteration:
            return None
'''
a = time.time()
pf = ParseFasta('G:\\Work\\input.fasta')
tuple = pf.getRecord()
while tuple is not None:
    #print tuple[1]
    tuple = pf.getRecord()
    
b= time.time()
print b-a
'''