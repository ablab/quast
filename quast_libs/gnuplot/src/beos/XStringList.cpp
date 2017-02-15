Class XStringList {
	protected:
		int32	numStrings;
		int32	maxStrings;
		int32	bufferSize;
		int32	bufferUse;
		char 	*strArray[];
		char	*strBuffer;
		char	*strNext;
	public :
		XStringList();
		XStringList(char *head, int32 numStr);
		~XStringList();
		Add(char *str);
		char *operator[];
}
