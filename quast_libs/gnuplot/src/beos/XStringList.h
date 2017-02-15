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
		char *operator[int i];
}

XStringList::XStringList()
{
	numStrings	= 0;
	maxStrings	= INITIAL_SIZE;
	bufferSize	= INITIAL_SIZE * 16;
	bufferUse	= 0;
	strArray	new();
	strNext = strBuffer = ;
}

XStringList::XStringList(char *head, int32 numStr)
{

}

XStringList::~XStringList()
{

}

XStringList::Add(char *str)
{

}

char *XStringList::operator[int idx]
{

}

