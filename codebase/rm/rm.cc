
#include "rm.h"

RelationManager* RelationManager::instance()
{
    static RelationManager _rm;
    return &_rm;
}

RelationManager::RelationManager()
{
    ix_manager=IndexManager::instance();
}

RelationManager::~RelationManager()
{
}

RC RelationManager::createCatalog()
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle tableFileHandle, colFileHandle;
	DirDescription dirDescription;
	RC rc;
	dirDescription.slotCount = 1;
	dirDescription.freeSpacePointer = 0;
	int pageNum = 1;
	string tableName = "Tables";
	string colName = "Columns";

	// Create table "Tables"
	rbf_manager->createFile(tableName);
	rbf_manager->openFile(tableName, tableFileHandle);
	//cout << "old getNumberOfPages: " << tableFileHandle.getNumberOfPages() << endl;
	char *page = NULL;
	page = new char[PAGE_SIZE];
	memset(page, 0, PAGE_SIZE);
	memcpy(page+PAGE_SIZE-sizeof(dirDescription), &dirDescription, sizeof(dirDescription));
	tableFileHandle.appendPage(page);
	//tableFileHandle.getNumberOfPages();
	//cout << "new getNumberOfPages: " << tableFileHandle.getNumberOfPages() << endl;
	// Insert the Tables amd Columns tuple to "Tables"
	rc = insertTableTuple(tableFileHandle, tableName, pageNum);
	rc = insertTableTuple(tableFileHandle, colName, pageNum);
	rbf_manager->closeFile(tableFileHandle);
	//page = NULL;
	delete []page;

	// Create table "Columns"
	rbf_manager->createFile(colName);
	rbf_manager->openFile(colName, colFileHandle);
	dirDescription.slotCount = 1;
	dirDescription.freeSpacePointer = 0;
	char *colPage = NULL;
	colPage = new char[PAGE_SIZE];
	memset(colPage, 0, PAGE_SIZE);
	memcpy(colPage+PAGE_SIZE-sizeof(dirDescription), &dirDescription, sizeof(dirDescription));
	colFileHandle.appendPage(colPage);
	colFileHandle.getNumberOfPages();
	rbf_manager->closeFile(colFileHandle);
	//colPage = NULL;
	delete []colPage;

	// Insert attrs of "Tables" to "Columns"
	vector<Attribute> tableAttrs;
	Attribute attr;
	attr.name = "table-id";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	tableAttrs.push_back(attr);

	attr.name = "table-name";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)50;
	tableAttrs.push_back(attr);

	attr.name = "file-name";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)50;
	tableAttrs.push_back(attr);

	// Insert attr of "Columns" to "Columns"
	vector<Attribute> colAttrs;
	attr.name = "table-id";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)50;
	colAttrs.push_back(attr);

	attr.name = "column-name";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)50;
	colAttrs.push_back(attr);

	attr.name = "column-type";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	colAttrs.push_back(attr);

	attr.name = "column-length";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	colAttrs.push_back(attr);

	attr.name = "column-position";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	colAttrs.push_back(attr);
    
    attr.name = "indexed";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    colAttrs.push_back(attr);

	rbf_manager->openFile(colName, colFileHandle);
	for(int pos=1; pos<=tableAttrs.size(); pos++){
		rc = insertColTuple(colFileHandle, tableAttrs[pos-1], pageNum, pos, 0);
	}

	//table_id = 2;
	colPage = new char[PAGE_SIZE];
	memset(colPage, 0, PAGE_SIZE);
	colFileHandle.readPage(1, colPage);
	memcpy(&dirDescription, colPage+PAGE_SIZE-sizeof(DirDescription), sizeof(DirDescription));
	dirDescription.slotCount ++;
	memcpy(colPage+PAGE_SIZE-sizeof(dirDescription), &dirDescription, sizeof(dirDescription));
	colFileHandle.writePage(pageNum, colPage);
	for(int pos=1; pos<=colAttrs.size(); pos++){
		rc = insertColTuple(colFileHandle, colAttrs[pos-1], pageNum, pos, 0);
	}

	colFileHandle.readPage(1, colPage);
	memcpy(&dirDescription, colPage+PAGE_SIZE-sizeof(DirDescription), sizeof(DirDescription));
	dirDescription.slotCount++;
	memcpy(colPage+PAGE_SIZE-sizeof(dirDescription), &dirDescription, sizeof(dirDescription));
	colFileHandle.writePage(pageNum, colPage);
	rbf_manager->closeFile(colFileHandle);
    delete []colPage;
    return 0;
}

RC RelationManager::deleteCatalog()
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	rbf_manager->destroyFile("Tables");
	rbf_manager->destroyFile("Columns");
    return 0;
}

bool RelationManager::tableExist(const string &tableName){
	struct stat stFileInfo;

	if(stat(tableName.c_str(), &stFileInfo) == 0) return true;
	else return false;
}

RC RelationManager::insertTableTuple(FileHandle &fileHandle, const string &tableName, int pageNum){
	char* page = NULL;
	page = new char[PAGE_SIZE];
	memset(page, 0, PAGE_SIZE);
	//cout << "tableName: " << tableName << endl;
	//cout << "insertTableTuple getNumberOfPages: " << fileHandle.getNumberOfPages() << endl;
	//cout << "pageNum: " << pageNum << endl;
    if(fileHandle.readPage(pageNum,page)!=0){
    		//cout << "tableName: " << tableName << endl;
        cout << "readPage from insertTableTuple fail!" << endl << endl;
        return -1;
    }
	DirDescription dirDescription;
	memcpy(&dirDescription,page+PAGE_SIZE-sizeof(DirDescription),sizeof(DirDescription));
	short tableSize = sizeof(int) + sizeof(short) + tableName.length()*2;
	const char* name = tableName.c_str();

	//cout << "table_id: " << dirDescription.slotCount << endl;
	int table_id = (int)dirDescription.slotCount;
	memcpy(page+dirDescription.freeSpacePointer, &table_id, sizeof(int));
	short length = tableName.size();
	//cout << "length: " << length << endl;
	memcpy(page+dirDescription.freeSpacePointer+sizeof(int), &length, sizeof(short));
	memcpy(page+dirDescription.freeSpacePointer+sizeof(int)+sizeof(short), name, length);
	memcpy(page+dirDescription.freeSpacePointer+sizeof(int)+sizeof(short)+length, name, length);

	// update dirDescription
	dirDescription.slotCount++;
	dirDescription.freeSpacePointer += tableSize;
	memcpy(page+PAGE_SIZE-sizeof(DirDescription), &dirDescription, sizeof(DirDescription));
	fileHandle.writePage(pageNum,page);

	// update direction page
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	int freeSpace = PAGE_SIZE - sizeof(DirDescription) - dirDescription.freeSpacePointer;
	RID rid;
	rid.pageNum = pageNum;
	rid.slotNum = 0;
	rbf_manager->updateDirectoryPage(fileHandle, rid, freeSpace);

	//page = NULL;
	delete []page;
	return 0;
}

RC RelationManager::insertColTuple(FileHandle &fileHandle, const Attribute &attr, int pageNum, int pos, int indexed){
	char* page = NULL;
	page = new char[PAGE_SIZE];
	memset(page, 0, PAGE_SIZE);
	//cout << "pageNum: " << pageNum << endl;
    if(fileHandle.readPage(pageNum,page)!=0){
        cout<<"readPage from insertColTuple fail!"<<endl;
        return -1;
    }
	DirDescription dirDescription;
	memcpy(&dirDescription,page+PAGE_SIZE-sizeof(DirDescription),sizeof(DirDescription));
	short colSize = 0;
	short varcharLength;
	const char* attrName = attr.name.c_str();
	//cout << "attrName: " << attrName << endl;

	//cout << "table_id: " << dirDescription.slotCount << endl;
	//cout << "freeSpacePointer: " << dirDescription.freeSpacePointer << endl;
	int table_id = (int)dirDescription.slotCount;
	memcpy(page+dirDescription.freeSpacePointer+colSize, &table_id, sizeof(int));
	colSize += sizeof(int);
	varcharLength = attr.name.length();
	memcpy(page+dirDescription.freeSpacePointer+colSize, &varcharLength, sizeof(short));
	colSize += sizeof(short);
	memcpy(page+dirDescription.freeSpacePointer+colSize, attrName, varcharLength);
	//cout<<"varcharLength: "<<varcharLength<<endl;
	//cout << "attr.name: " << attr.name<< endl;
	colSize += varcharLength;
	memcpy(page+dirDescription.freeSpacePointer+colSize, &attr.type, sizeof(int));
	colSize += sizeof(int);
	memcpy(page+dirDescription.freeSpacePointer+colSize, &attr.length, sizeof(int));
	colSize += sizeof(int);
	memcpy(page+dirDescription.freeSpacePointer+colSize, &pos, sizeof(int));
	colSize += sizeof(int);
    memcpy(page+dirDescription.freeSpacePointer+colSize, &indexed, sizeof(int));
    colSize += sizeof(int);

	// update dirDescription
	dirDescription.freeSpacePointer += colSize;
	memcpy(page+PAGE_SIZE-sizeof(DirDescription), &dirDescription, sizeof(DirDescription));
	fileHandle.writePage(pageNum,page);

	// update direction page
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	int freeSpace = PAGE_SIZE - sizeof(DirDescription) - dirDescription.freeSpacePointer;
	RID rid;
	rid.pageNum = pageNum;
	rid.slotNum = 0;
	rbf_manager->updateDirectoryPage(fileHandle, rid, freeSpace);

	//page = NULL;
	delete []page;
	return 0;
}

RC RelationManager::createTable(const string &tableName, const vector<Attribute> &attrs)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle, tableFileHandle, colFileHandle;
	RC rc;
	DirDescription dirDescription;
	rbf_manager->createFile(tableName);

	int tableSize;
	int colSize=0;
	int pageNum;
	// Insert tuple to table "Tables"
	rbf_manager->openFile("Tables", tableFileHandle);
	//cout << "getNumberOfPages: " << tableFileHandle.getNumberOfPages() << endl;
	tableSize = sizeof(int) + sizeof(short) + tableName.length()*2;
	pageNum = rbf_manager->getFreePage(tableFileHandle, tableSize);
	//cout << "pageNum: " << pageNum << endl;
	insertTableTuple(tableFileHandle, tableName, pageNum);
	rbf_manager->closeFile(tableFileHandle);

	// Insert tuple to table "Columns"
	rbf_manager->openFile("Columns", colFileHandle);
	for(vector<Attribute>::const_iterator i=attrs.begin(); i!=attrs.end(); i++){
		colSize = colSize + sizeof(int) + sizeof(short) + i->name.length() + sizeof(int)*3;
	}
	pageNum = rbf_manager->getFreePage(colFileHandle, colSize);
	//cout << "pageNum: " << pageNum << endl;
	for(int pos=1; pos<=attrs.size(); pos++){
		rc = insertColTuple(colFileHandle, attrs[pos-1], pageNum, pos, 0);
	}
	// update table id
	char *colPage = NULL;
	colPage = new char[PAGE_SIZE];
	memset(colPage, 0, PAGE_SIZE);
	colFileHandle.readPage(pageNum, colPage);
	memcpy(&dirDescription, colPage+PAGE_SIZE-sizeof(DirDescription), sizeof(DirDescription));
	dirDescription.slotCount ++;
	memcpy(colPage+PAGE_SIZE-sizeof(dirDescription), &dirDescription, sizeof(dirDescription));
	colFileHandle.writePage(pageNum, colPage);
    delete []colPage;
	rbf_manager->closeFile(colFileHandle);
	//table_id++;
    return 0;
}

RC RelationManager::deleteTable(const string &tableName)
{
	if(tableName.compare("Tables")==0 or tableName.compare("Columns")==0){
		cout << "System catalog cannot be deleted!" << endl;
		return -1;
	}
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle tableFileHandle, colFileHandle;
	DirDescription dirDescription;
	RID rid;
	int offset, beginOffset, shiftLength, recordSize;
	int findID, id;
	int stop=0;
	short varcharLength;
	char* name = NULL;
	const char* TableName = tableName.c_str();
	rbf_manager->destroyFile(tableName);

	rbf_manager->openFile("Tables", tableFileHandle);
	char* tablePage = NULL;
	for(int pageNum=1; pageNum<=tableFileHandle.getNumberOfPages(); pageNum++){
		tablePage = new char[PAGE_SIZE];
		memset(tablePage, 0, PAGE_SIZE);
		if(tableFileHandle.readPage(pageNum,tablePage)!=0){
			//cout<<"readPage from getAttributes fail!"<<endl;
			return -1;
		}
		offset = 0;
		// find table offset
		while(true){
			memcpy(&id, tablePage+offset, sizeof(int));
			//cout << "id: " << id << endl;
			offset += sizeof(int);
			memcpy(&varcharLength, tablePage+offset, sizeof(short));
			//cout << "varcharLength: " << varcharLength << endl;
			offset += sizeof(short);
			name = new char[varcharLength];
			memset(name, 0, varcharLength+1);
			memcpy(name, tablePage+offset, varcharLength);
			//cout << "tableName in Tables: " << name << endl;
			offset += varcharLength;
			if(memcmp(name, TableName, tableName.length())==0)
				break;
			offset += varcharLength;
			name = NULL;
			delete []name;
			// Not in this page
			if(offset > PAGE_SIZE-sizeof(DirDescription))
				break;
		}
		// delete record in "Tables" and shift
		if(memcmp(name, TableName, tableName.length())==0){
			memcpy(&dirDescription, tablePage+PAGE_SIZE-sizeof(DirDescription), sizeof(DirDescription));
			beginOffset = offset - (varcharLength+sizeof(short)+sizeof(int));
			offset += varcharLength;
			recordSize = offset - beginOffset;
			shiftLength = dirDescription.freeSpacePointer - offset;
			char* shiftContent = new char[shiftLength];
			memset(shiftContent, 0, shiftLength);
			memcpy(shiftContent, tablePage+offset, shiftLength);
			memcpy(tablePage+beginOffset, shiftContent, shiftLength);
			dirDescription.freeSpacePointer -= recordSize;
			memcpy(tablePage+dirDescription.freeSpacePointer, &stop, sizeof(int));
			memcpy(tablePage+PAGE_SIZE-sizeof(DirDescription), &dirDescription, sizeof(DirDescription));
			tableFileHandle.writePage(pageNum, tablePage);
			rid.pageNum = pageNum;
			rid.slotNum = 0;
			shiftContent = NULL;
			delete []shiftContent;
			tablePage = NULL;
			delete []tablePage;
			name = NULL;
			delete []name;
			break;
		}
		tablePage = NULL;
		delete []tablePage;
	}
	// update direction page
	int freeSpace = PAGE_SIZE - sizeof(DirDescription) - dirDescription.freeSpacePointer;
	rbf_manager->updateDirectoryPage(tableFileHandle, rid, freeSpace);

	rbf_manager->closeFile(tableFileHandle);
	//cout << "delete record " << tableName << " in Tables success" << endl;

    rbf_manager->openFile("Columns", colFileHandle);
    int colPageNum;
    char* colPage = NULL;
    char* shiftContent = NULL;
    for(int pageNum=1; pageNum<=colFileHandle.getNumberOfPages(); pageNum++){
    		colPage = new char[PAGE_SIZE];
    		memset(colPage, 0, PAGE_SIZE);
		if(colFileHandle.readPage(pageNum,colPage)!=0){
			cout<<"readPage from getAttributes fail!"<<endl;
			return -1;
		}
		// find position(offset) of table in "Columns" by table_id
		offset = 0;
		while(true){
			memcpy(&findID, colPage+offset, sizeof(int));
			//cout << "findID: " << findID << endl;
			offset += sizeof(int);
			if(findID==id){
				colPageNum = pageNum;
				break;
			}
			memcpy(&varcharLength, colPage+offset, sizeof(short));
			//cout << "varcharLength: " << varcharLength << endl;
			offset += sizeof(short) + varcharLength + sizeof(int)*4;
			// not in this page
			if(offset > PAGE_SIZE-sizeof(DirDescription))
				break;
		}
		if(findID==id)
			break;
		colPage = NULL;
		delete []colPage;
    }

	// delete record in "Columns" and shift
	while(findID==id){
		//cout << "findID: " << findID << endl;
		memcpy(&dirDescription, colPage+PAGE_SIZE-sizeof(DirDescription), sizeof(DirDescription));
		beginOffset = offset - sizeof(int);
		//cout << "beginOffset: " << beginOffset << endl;
		memcpy(&varcharLength, colPage+offset, sizeof(short));
		//cout << "varcharLength: " << varcharLength << endl;
		offset += sizeof(short) + varcharLength + sizeof(int)*3;
		//cout << "offset: " << offset << endl;
		recordSize = offset - beginOffset;
		//cout << "recordSize: " << recordSize << endl;
		shiftLength = dirDescription.freeSpacePointer - offset;
		//cout << "shiftLength: " << shiftLength << endl;
		shiftContent = new char[shiftLength];
		memset(shiftContent, 0, shiftLength);
		memcpy(shiftContent, colPage+offset, shiftLength);
		memcpy(colPage+beginOffset, shiftContent, shiftLength);
		dirDescription.freeSpacePointer -= recordSize;
		//cout << "freeSpacePointer: " << dirDescription.freeSpacePointer << endl;
		memcpy(colPage+dirDescription.freeSpacePointer, &stop, sizeof(int));
		memcpy(colPage+PAGE_SIZE-sizeof(DirDescription), &dirDescription, sizeof(DirDescription));
		colFileHandle.writePage(colPageNum, colPage);
		rid.pageNum = colPageNum;
		rid.slotNum = 0;
		shiftContent = NULL;
		delete []shiftContent;
		memcpy(&findID, colPage+beginOffset, sizeof(int));
		//cout << "findID: " << findID << endl;
		offset = beginOffset + sizeof(int);
	}

	// update direction page
	freeSpace = PAGE_SIZE - sizeof(DirDescription) - dirDescription.freeSpacePointer;
	rbf_manager->updateDirectoryPage(colFileHandle, rid, freeSpace);

    rbf_manager->closeFile(colFileHandle);
    //cout << "delete record " << tableName << " in Columns success" << endl;
    return 0;
}

RC RelationManager::getAttributes(const string &tableName, vector<Attribute> &attrs)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle tableFileHandle, colFileHandle;
	Attribute attr;
	int offset, id, findID;
	short varcharLength;
	char* name = NULL;
	const char* TableName = tableName.c_str();

	rbf_manager->openFile("Tables", tableFileHandle);

	// find tableName is in which page of "Tables" and its table_id
	//cout << "tableFileHandle.getNumberOfPages: " << tableFileHandle.getNumberOfPages() << endl;
	//cout << "tableName: " << tableName << endl;
	for(int pageNum=1; pageNum<=tableFileHandle.getNumberOfPages(); pageNum++){
		char* tablePage = NULL;
		tablePage = new char[PAGE_SIZE];
		memset(tablePage, 0, PAGE_SIZE);
		if(tableFileHandle.readPage(pageNum,tablePage)!=0){
			//cout << "pageNum: " << pageNum << endl;
			//cout<<"readPage from getAttributes fail!"<<endl;
			return -1;
		}
		offset = 0;
		// Find table id by "Tables"
		while(true){
			memcpy(&id, tablePage+offset, sizeof(int));
			//cout << "id: " << id << endl;
			offset += sizeof(int);
			memcpy(&varcharLength, tablePage+offset, sizeof(short));
			//cout << "varcharLength: " << varcharLength << endl;
			offset += sizeof(short);
			name = NULL;
			name = new char[varcharLength];
			memset(name, 0, varcharLength+1);
			memcpy(name, tablePage+offset, varcharLength);
			string str_name = name;
			//cout << "strName: " << str_name << endl;
			//cout << "tableName in Tables: " << name << "," << TableName << endl;
			//cout << name << endl;
			offset += varcharLength;
			//cout << "cmp: " << memcmp(name, TableName, tableName.length()) << endl;
			if(memcmp(name, TableName, tableName.length())==0){
				break;
			}
			offset += varcharLength;
			//name = NULL;
			delete []name;
			// Not in this page
			if(offset > PAGE_SIZE-sizeof(DirDescription))
				break;
		}
		if(memcmp(name, TableName, tableName.length())==0){
			//tablePage = NULL;
			delete []tablePage;
			//name = NULL;
			delete []name;
			break;
		}
		tablePage = NULL;
		delete []tablePage;
	}
	rbf_manager->closeFile(tableFileHandle);

    // Find attrs by "Columns"
    rbf_manager->openFile("Columns", colFileHandle);
    char* colPage = NULL;
    for(int pageNum=1; pageNum<=colFileHandle.getNumberOfPages(); pageNum++){
    		colPage = new char[PAGE_SIZE];
    		memset(colPage, 0, PAGE_SIZE);
		if(colFileHandle.readPage(pageNum,colPage)!=0){
			//cout << "pageNum???: " << pageNum << endl;
			//cout<<"readPage from getAttributes fail!"<<endl;
			return -1;
		}
		// find position(offset) of table in "Columns" by table_id
		offset = 0;
		while(true){
            //cout<<"offset: "<<offset<<endl;
			memcpy(&findID, colPage+offset, sizeof(int));
			//cout << "findID: " << findID << endl;
			offset += sizeof(int);
			if(findID==id)
				break;
			memcpy(&varcharLength, colPage+offset, sizeof(short));
			//cout << "varcharLength: " << varcharLength << endl;
			offset += sizeof(short) + varcharLength + sizeof(int)*4;
			// not in this page
			if(offset > PAGE_SIZE-sizeof(DirDescription))
				break;
		}
		if(findID==id)
			break;
		//colPage = NULL;
		delete []colPage;
    }
	// find attrs

    int pos;
	while(findID==id){
		char* attrName = NULL;
		attrName = new char[varcharLength];
		memcpy(&varcharLength, colPage+offset, sizeof(short));
		//cout << "varcharLength: " << varcharLength << endl;
		offset += sizeof(short);
		memset(attrName, 0, varcharLength+1);
		memcpy(attrName, colPage+offset, varcharLength);
		//cout << "attrName: " << attrName << endl;
		attr.name = attrName;
		//attrName=NULL;
		//cout << "name: " << attr.name << endl;
		offset += varcharLength;
		memcpy(&attr.type, colPage+offset, sizeof(int));
		offset += sizeof(int);
		memcpy(&attr.length, colPage+offset, sizeof(int));
		offset += sizeof(int);
		memcpy(&pos, colPage+offset, sizeof(int));
		//cout << "pos: " << pos << endl;
		offset += sizeof(int);
        offset += sizeof(int);//indexed
		attrs.push_back(attr);
		memcpy(&findID, colPage+offset, sizeof(int));
		offset += sizeof(int);
		//cout << "findID: " << findID << endl;
        delete []attrName;
	}
	colPage = NULL;
	delete []colPage;
    rbf_manager->closeFile(colFileHandle);
    return 0;
}

RC RelationManager::insertTuple(const string &tableName, const void *data, RID &rid)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	RC rc;
	vector<Attribute> recordDescriptor;
	rc = rbf_manager->openFile(tableName, fileHandle);
	if(rc){
		//cout << "openFile in insertTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	getAttributes(tableName, recordDescriptor);
	// test recordDescriptor
	/*
    for(unsigned i = 0; i < recordDescriptor.size(); i++){
        cout << (i+1) << ". Attr Name: " << recordDescriptor[i].name << " Type: " << (AttrType) recordDescriptor[i].type << " Len: " << recordDescriptor[i].length << endl;
    }
    */
	rc = rbf_manager->insertRecord(fileHandle, recordDescriptor, data, rid);
	if(rc){
		//cout << "insertRecord in insertTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	rc = rbf_manager->closeFile(fileHandle);
	if(rc){
		//cout << "closeFile in insertTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    
    int indexed=0;
    int nullIndicatorSize=ceil((double)recordDescriptor.size()/CHAR_BIT);
    int offset=nullIndicatorSize;
    for(int i=0;i<recordDescriptor.size();i++){
        string ix_Name(tableName+"_"+recordDescriptor[i].name+"_ix");
        IXFileHandle ixfileHandle;
        if(ix_manager->openFile(ix_Name,ixfileHandle)!=0){
            switch(recordDescriptor[i].type){
                case TypeInt:
                    offset+=sizeof(int);
                    break;
                case TypeReal:
                    offset+=sizeof(float);
                    break;
                case TypeVarChar:
                    int length;
                    memcpy(&length,data,sizeof(int));
                    offset+=length+sizeof(int);
            }
            continue;
        }
        void* indexData;
        switch(recordDescriptor[i].type){
            case TypeInt:
                indexData=malloc(sizeof(int));
                memcpy(indexData,(char*)data+offset,sizeof(int));
                offset+=sizeof(int);
                break;
            case TypeReal:
                indexData=malloc(sizeof(float));
                memcpy(indexData,(char*)data+offset,sizeof(float));
                offset+=sizeof(float);
                break;
            case TypeVarChar:
                int length;
                memcpy(&length,data,sizeof(int));
                indexData=malloc(length+sizeof(int));
                memcpy(indexData,(char*)data+offset,length+sizeof(int));
                offset+=length+sizeof(int);
        }
        ix_manager->insertEntry(ixfileHandle, recordDescriptor[i], indexData, rid);
        ix_manager->closeFile(ixfileHandle);
        indexed++;
    }
    return 0;
}

RC RelationManager::deleteTuple(const string &tableName, const RID &rid)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	RC rc;
	vector<Attribute> recordDescriptor;
	rc = rbf_manager->openFile(tableName, fileHandle);
	if(rc){
		cout << "openFile in deleteTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	//cout << "openFile suc in deleteTuple!" << endl;
	getAttributes(tableName, recordDescriptor);
	//cout << "getAttr suc in deleteTuple!" << endl;
	rc = rbf_manager->deleteRecord(fileHandle, recordDescriptor, rid);
	//cout << "delete suc!" << endl;
	if(rc){
		cout << "deleteRecord in deleteTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	rc = rbf_manager->closeFile(fileHandle);
	if(rc){
		cout << "closeFile in deleteTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    return 0;
}

RC RelationManager::updateTuple(const string &tableName, const void *data, const RID &rid)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	RC rc;
	vector<Attribute> recordDescriptor;
	rc = rbf_manager->openFile(tableName, fileHandle);
	if(rc){
		cout << "openFile in updateTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	getAttributes(tableName, recordDescriptor);
	rc = rbf_manager->updateRecord(fileHandle, recordDescriptor, data, rid);
	//cout << "updateRecord suc!" << endl;
	if(rc){
		cout << "updateRecord in updateTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	rc = rbf_manager->closeFile(fileHandle);
	if(rc){
		cout << "closeFile in updateTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    return 0;
}

RC RelationManager::readTuple(const string &tableName, const RID &rid, void *data)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	RC rc;
	vector<Attribute> recordDescriptor;
	rc = rbf_manager->openFile(tableName, fileHandle);
	//cout << "open RM success" << endl;
	if(rc){
		cout << "openFile in readTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	//cout << tableName << endl;
	getAttributes(tableName, recordDescriptor);
	//cout << "getAttributes success" << endl;
	/*
    for(unsigned i = 0; i < recordDescriptor.size(); i++)
    {
        cout << (i+1) << ". Attr Name: " << recordDescriptor[i].name << " Type: " << (AttrType) recordDescriptor[i].type << " Len: " << recordDescriptor[i].length << endl;
    }
    */

	rc = rbf_manager->readRecord(fileHandle, recordDescriptor, rid, data);
	if(rc){
		cout << "readRecord in readTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	//cout << "read RM success" << endl;
	rc = rbf_manager->closeFile(fileHandle);
	if(rc){
		cout << "closeFile in readTuple in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    return 0;
}

RC RelationManager::printTuple(const vector<Attribute> &attrs, const void *data)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	rbf_manager->printRecord(attrs, data);
	return 0;
}

RC RelationManager::readAttribute(const string &tableName, const RID &rid, const string &attributeName, void *data)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	RC rc;
	vector<Attribute> recordDescriptor;
	rc = rbf_manager->openFile(tableName, fileHandle);
	if(rc){
		cout << "openFile in readAttribute in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	getAttributes(tableName, recordDescriptor);
	rc = rbf_manager->readAttribute(fileHandle, recordDescriptor, rid, attributeName, data);
	if(rc){
		cout << "readAttribute in readAttribute in Table: " << tableName << " failed!" << endl;
		return -1;
	}
	rc = rbf_manager->closeFile(fileHandle);
	if(rc){
		cout << "closeFile in readAttribute in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    return 0;
}

RC RelationManager::scan(const string &tableName,
      const string &conditionAttribute,
      const CompOp compOp,                  
      const void *value,                    
      const vector<string> &attributeNames,
      RM_ScanIterator &rm_ScanIterator)
{
	RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
	FileHandle fileHandle;
	RC rc;
	vector<Attribute> recordDescriptor;
	rc = rbf_manager->openFile(tableName, fileHandle);
	if(rc){
		//cout << "openFile in scan in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    
	getAttributes(tableName, recordDescriptor);
    //cout<<"filehandle.getNumber: "<<fileHandle.getNumberOfPages()<<endl;
	rc = rbf_manager->scan(fileHandle, recordDescriptor, conditionAttribute, compOp, value, attributeNames, rm_ScanIterator.rbfm_iterator);
	if(rc){
		cout << "scan in Table: " << tableName << " failed!" << endl;
		return -1;
	}
    return 0;
}

RC RelationManager::indexScan(const string &tableName,
                              const string &attributeName,
                              const void *lowKey,
                              const void *highKey,
                              bool lowKeyInclusive,
                              bool highKeyInclusive,
                              RM_IndexScanIterator &rm_IndexScanIterator)
{
    string idxName(tableName+"_"+attributeName+"_ix");
    //cout<<"idxname: "<<idxName<<" attributeName: "<<attributeName<<endl;
    IXFileHandle ixfileHandle;
    if(ix_manager->openFile(idxName,ixfileHandle)!=0){
        return -1;
    }
    vector<Attribute> attrs;
    getAttributes(tableName, attrs);
    Attribute attr;
    for (int i=0; i < attrs.size(); ++i) {
        if (attrs[i].name.compare(attributeName) == 0) {
            attr = attrs[i];
        }
    }
    ix_manager->scan(ixfileHandle,attr,lowKey,highKey,lowKeyInclusive,highKeyInclusive,rm_IndexScanIterator.ix_ScanIterator);
}
// Extra credit work
RC RelationManager::dropAttribute(const string &tableName, const string &attributeName)
{
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const string &tableName, const Attribute &attr)
{
    return -1;
}

RC RelationManager::createIndex(const string &tableName, const string &attributeName){
    string ix_Name(tableName+"_"+attributeName+"_ix");
    if(ix_manager->createFile(ix_Name)!=0){
        return -1;
    }
    vector<Attribute> attrs;
    Attribute attr;
    if(getAttributes(tableName,attrs)!=0){
        return -1;
    }
    for(int i=0;i<attrs.size();i++){
        if(attrs[i].name.compare(attributeName)==0){
            attr=attrs[i];
            //cout<<"Found attr: "<<attr.name<<endl;
        }
    }
    RM_ScanIterator rmsi;
    vector<string> attributeNames;
    attributeNames.push_back(attributeName);
    if(scan(tableName,"", NO_OP, NULL, attributeNames, rmsi)!=0){
        return -1;
    }
    
    IXFileHandle ixfileHandle;
    if(ix_manager->openFile(ix_Name,ixfileHandle)!=0){
        return -1;
    }
    
    RID rid;
    void *returnedData;
    if(attr.type==TypeVarChar)
        returnedData=malloc(attr.length+sizeof(int));
    else
        returnedData=malloc(attr.length);
    while(rmsi.getNextTuple(rid, returnedData) != RM_EOF)
    {
        ix_manager->insertEntry(ixfileHandle, attr, returnedData, rid);
    }
    
    free(returnedData);
    
    updateCatalog(tableName,attributeName,1);
    return 0;
    
}

RC RelationManager::destroyIndex(const string &tableName, const string &attributeName){
    string idxName(tableName+"_"+attributeName+"_ix");
    ix_manager->destroyFile(idxName);
    updateCatalog(tableName,attributeName,0);
    return 0;
}

RC RelationManager::updateCatalog(const string &tableName, const string &attributeName, int indexed){
    RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
    FileHandle tableFileHandle, colFileHandle;
    Attribute attr;
    int offset, id, findID;
    short varcharLength;
    char* name = NULL;
    const char* TableName = tableName.c_str();
    
    rbf_manager->openFile("Tables", tableFileHandle);

    for(int pageNum=1; pageNum<=tableFileHandle.getNumberOfPages(); pageNum++){
        char* tablePage = NULL;
        tablePage = new char[PAGE_SIZE];
        memset(tablePage, 0, PAGE_SIZE);
        if(tableFileHandle.readPage(pageNum,tablePage)!=0){
            return -1;
        }
        offset = 0;
        // Find table id by "Tables"
        while(true){
            memcpy(&id, tablePage+offset, sizeof(int));
            offset += sizeof(int);
            memcpy(&varcharLength, tablePage+offset, sizeof(short));
            offset += sizeof(short);
            name = NULL;
            name = new char[varcharLength];
            memset(name, 0, varcharLength+1);
            memcpy(name, tablePage+offset, varcharLength);
            string str_name = name;

            offset += varcharLength;

            if(memcmp(name, TableName, tableName.length())==0){
                break;
            }
            offset += varcharLength;
            delete []name;
            // Not in this page
            if(offset > PAGE_SIZE-sizeof(DirDescription))
                break;
        }
        if(memcmp(name, TableName, tableName.length())==0){
            delete []tablePage;
            delete []name;
            break;
        }
        delete []tablePage;
    }
    rbf_manager->closeFile(tableFileHandle);
    
    // Find attrs by "Columns"
    rbf_manager->openFile("Columns", colFileHandle);
    char* colPage = NULL;
    int pageNum;
    for(pageNum=1; pageNum<=colFileHandle.getNumberOfPages(); pageNum++){
        colPage = new char[PAGE_SIZE];
        memset(colPage, 0, PAGE_SIZE);
        if(colFileHandle.readPage(pageNum,colPage)!=0){
            return -1;
        }
        // find position(offset) of table in "Columns" by table_id
        offset = 0;
        while(true){
            memcpy(&findID, colPage+offset, sizeof(int));
            offset += sizeof(int);
            if(findID==id)
                break;
            memcpy(&varcharLength, colPage+offset, sizeof(short));
            offset += sizeof(short) + varcharLength + sizeof(int)*4;
            // not in this page
            if(offset > PAGE_SIZE-sizeof(DirDescription))
                break;
        }
        if(findID==id)
            break;
        delete []colPage;
    }
    // find attrs
    
    while(findID==id){
        char* attrName = NULL;
        attrName = new char[varcharLength];
        memcpy(&varcharLength, colPage+offset, sizeof(short));
        offset += sizeof(short);
        memset(attrName, 0, varcharLength+1);
        memcpy(attrName, colPage+offset, varcharLength);
        offset += varcharLength;
        //memcpy(&attr.type, colPage+offset, sizeof(int));
        offset += sizeof(int);
        //memcpy(&attr.length, colPage+offset, sizeof(int));
        offset += sizeof(int);
        //memcpy(&pos, colPage+offset, sizeof(int));
        offset += sizeof(int);
        if(memcmp(attrName, attributeName.c_str(), attributeName.length())==0){
            memcpy(colPage+offset,&indexed,sizeof(int));
        }
        offset += sizeof(int);//indexed
        //attrs.push_back(attr);
        memcpy(&findID, colPage+offset, sizeof(int));
        offset += sizeof(int);
        delete []attrName;
    }
    
    colFileHandle.writePage(pageNum,colPage);
    //colPage = NULL;
    delete []colPage;
    rbf_manager->closeFile(colFileHandle);
    return 0;

}


