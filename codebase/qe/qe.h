#ifndef _qe_h_
#define _qe_h_

#include <vector>

#include "../rbf/rbfm.h"
#include "../rm/rm.h"
#include "../ix/ix.h"
#include <map>

#define QE_EOF (-1)  // end of the index scan

using namespace std;

typedef enum{ MIN=0, MAX, COUNT, SUM, AVG } AggregateOp;

// The following functions use the following
// format for the passed data.
//    For INT and REAL: use 4 chars
//    For VARCHAR: use 4 chars for the length followed by the characters

struct Value {
    AttrType type;          // type of value
    void     *data;         // value
};

class Pair {
public:
    int offset;
    int length;
    Pair(int offset, int length) {
        this->offset = offset;
        this->length = length;
    }

    Pair& operator=(const Pair &p) {
        this->offset = p.offset;
        this->length = p.length;
        return *this;
    }

    bool operator==(const Pair &p) const {
        if(this->offset == p.offset && this->length == p.length)
            return true;
        else return false;
    }
    static void combineData(char* leftData, vector<Attribute> &leftAttrs, char* rightData, vector<Attribute> &rightAttrs, char* combinedData){
        int totalSize = leftAttrs.size() + rightAttrs.size();
        int offset = ceil((double)totalSize/CHAR_BIT);
        
        int short standardNullIndicator = 0x80;
        int short nullIndicator = 0;
        int short nullIndicatorOffset = 0;
        
        int leftNullSize = ceil((double)leftAttrs.size()/CHAR_BIT);
        int rightNullSize = ceil((double)rightAttrs.size()/CHAR_BIT);
        int leftOffset = leftNullSize;
        int rightOffset = rightNullSize;
        
        int short rightNullIndicator = 0;
        int rightNullIndicatorOffset = 0;
        
        
        memcpy(&nullIndicator,leftData,sizeof(char));
        nullIndicatorOffset++;
        for(int i=0;i<leftAttrs.size();i++){
            if(i!=0&&i%8==0){
                memcpy(combinedData+rightNullIndicatorOffset, &rightNullIndicator, sizeof(char));
                rightNullIndicatorOffset++;
                
                memcpy(&nullIndicator,leftData + nullIndicatorOffset,sizeof(char));
                standardNullIndicator = 0x80;
                nullIndicatorOffset++;
            }
            if((nullIndicator&standardNullIndicator)==0){
                switch(leftAttrs[i].type){
                    case TypeInt:
                        leftOffset += sizeof(int);
                        break;
                    case TypeReal:
                        leftOffset += sizeof(float);
                        break;
                    case TypeVarChar:
                        int varLength;
                        memcpy(&varLength,(char*) leftData + leftOffset,sizeof(int));
                        leftOffset += varLength;
                        leftOffset += sizeof(int);
                        break;
                }
            }else{
                rightNullIndicator |= standardNullIndicator;
            }
            standardNullIndicator = standardNullIndicator>>1;
        }
        
        if(standardNullIndicator == 0){
            memcpy(combinedData+rightNullIndicatorOffset, &rightNullIndicator, sizeof(char));
            rightNullIndicatorOffset++;
            rightNullIndicator = 0;
            standardNullIndicator = 0x80;
        }
        
        int short rightStandardNullIndicator = 0x80;
        nullIndicatorOffset = 0;
        nullIndicator = 0;
        
        for(int i=0;i<rightAttrs.size();i++){
            if(i%8==0){
                memcpy(&nullIndicator,rightData + nullIndicatorOffset,sizeof(char));
                rightStandardNullIndicator = 0x80;
                nullIndicatorOffset++;
            }
            if((nullIndicator&rightStandardNullIndicator)==0){
                switch(rightAttrs[i].type){
                    case TypeInt:
                        rightOffset += sizeof(int);
                        break;
                    case TypeReal:
                        rightOffset += sizeof(float);
                        break;
                    case TypeVarChar:
                        int varLength;
                        memcpy(&varLength,(char*) rightData + rightOffset,sizeof(int));
                        rightOffset += varLength;
                        rightOffset += sizeof(int);
                        break;
                        
                }
            }else{
                rightNullIndicator |= standardNullIndicator;
            }
            
            standardNullIndicator = standardNullIndicator >>1;
            rightStandardNullIndicator = rightStandardNullIndicator >>1;
            if(standardNullIndicator == 0){
                memcpy(combinedData+rightNullIndicatorOffset, &rightNullIndicator, sizeof(char));
                rightNullIndicator = 0;
                standardNullIndicator = 0x80;
                rightNullIndicatorOffset++;
            }
        }
        memcpy(combinedData+rightNullIndicatorOffset, &rightNullIndicator, sizeof(char));
        
        int leftDataLength = leftOffset-leftNullSize;
        memcpy(combinedData+offset,leftData+leftNullSize,leftDataLength);
        
        int rightDataLength = rightOffset - rightNullSize;
        memcpy(combinedData+offset+leftDataLength,rightData+rightNullSize,rightDataLength);
    };
};

class Key {
public:
    AttrType type;
    void* data;

    Key(AttrType type, void* data) {
        this->type = type;
        switch(type){
            case TypeInt:
                this->data = malloc(sizeof(int));
                memcpy(this->data, data, sizeof(int));
                break;
            case TypeReal:
                this->data = malloc(sizeof(float));
                memcpy(this->data, data, sizeof(float));
                break;
            case TypeVarChar:
                int rightLength;
                memcpy(&rightLength,data,sizeof(int));
                this->data =  malloc(rightLength + sizeof(int));
                memcpy(this->data, data, rightLength + sizeof(int));
                break;
        }
    }

    Key(const Key &p) {
        this->type = p.type;
        switch(type){
            case TypeInt:
                this->data = malloc(sizeof(int));
                memcpy(this->data, p.data, sizeof(int));
                break;
            case TypeReal:
                this->data = malloc(sizeof(int));
                memcpy(this->data, p.data, sizeof(float));
                break;
            case TypeVarChar:
                int rightLength;
                memcpy(&rightLength,data,sizeof(int));
                this->data = malloc(rightLength + sizeof(int));
                memcpy(this->data, p.data, rightLength + sizeof(int));
                break;
        }
    }

    ~Key() {
        free(data);
    }

    Key& operator=(const Key &p) {
        this->type = p.type;
        switch(type){
            case TypeInt:
                memcpy(this->data, p.data, sizeof(int));
                break;
            case TypeReal:
                memcpy(this->data, p.data, sizeof(float));
                break;
            case TypeVarChar:
                int rightLength;
                memcpy(&rightLength,p.data,sizeof(int));
                this->data = malloc(rightLength + sizeof(int));
                memcpy(this->data, p.data, rightLength + sizeof(int));
                break;
        }
        return *this;
    }

    bool operator==(const Key &p) const {
        switch(type){
            case TypeInt:
                return *(int*) (data) ==  *(int*) (p.data);
            case TypeReal:
                return *(float*) (data) == *(float*) (p.data);
            case TypeVarChar:
                int leftLength;
                memcpy(&leftLength,data,sizeof(int));
                string leftValue((char*)data + sizeof(int), leftLength);
                int rightLength;
                memcpy(&rightLength,p.data,sizeof(int));
                string rightValue((char*) (p.data) + sizeof(int), rightLength);
                return strcmp(leftValue.c_str(), rightValue.c_str()) == 0;
        }
    }

    bool operator>(const Key &p) const {
        switch(type){
            case TypeInt:
                return *(int*) (data) >  *(int*) (p.data);
            case TypeReal:
                return *(float*) (data) > *(float*) (p.data);
            case TypeVarChar:
                int leftLength;
                memcpy(&leftLength,data,sizeof(int));
                string leftValue((char*)data + sizeof(int), leftLength);
                int rightLength;
                memcpy(&rightLength,p.data,sizeof(int));
                string rightValue((char*) (p.data) + sizeof(int), rightLength);
                return strcmp(leftValue.c_str(), rightValue.c_str()) > 0;
        }
    }

    bool operator>=(const Key &p) const {
        switch(type){
            case TypeInt:
                return *(int*) (data) >=  *(int*) (p.data);
            case TypeReal:
                return *(float*) (data) >= *(float*) (p.data);
            case TypeVarChar:
                int leftLength;
                memcpy(&leftLength,data,sizeof(int));
                string leftValue((char*)data + sizeof(int), leftLength);
                int rightLength;
                memcpy(&rightLength,p.data,sizeof(int));
                string rightValue((char*) (p.data) + sizeof(int), rightLength);
                return strcmp(leftValue.c_str(), rightValue.c_str()) >= 0;
        }
    }

    bool operator<(const Key &p) const {
        switch(type){
            case TypeInt:
                return *(int*) (data) <  *(int*) (p.data);
            case TypeReal:
                return *(float*) (data) < *(float*) (p.data);
            case TypeVarChar:
                int leftLength;
                memcpy(&leftLength,data,sizeof(int));
                string leftValue((char*)data + sizeof(int), leftLength);
                int rightLength;
                memcpy(&rightLength,p.data,sizeof(int));
                string rightValue((char*) (p.data) + sizeof(int), rightLength);
                return strcmp(leftValue.c_str(), rightValue.c_str()) < 0;
        }
    }

    bool operator<=(const Key &p) const {
        switch(type){
            case TypeInt:
                return *(int*) (data) <=  *(int*) (p.data);
            case TypeReal:
                return *(float*) (data) <= *(float*) (p.data);
            case TypeVarChar:
                int leftLength;
                memcpy(&leftLength,data,sizeof(int));
                string leftValue((char*)data + sizeof(int), leftLength);
                int rightLength;
                memcpy(&rightLength,p.data,sizeof(int));
                string rightValue((char*) (p.data) + sizeof(int), rightLength);
                return strcmp(leftValue.c_str(), rightValue.c_str()) <= 0;
        }
    }
};

struct Condition {
    string  lhsAttr;        // left-hand side attribute
    CompOp  op;             // comparison operator
    bool    bRhsIsAttr;     // TRUE if right-hand side is an attribute and not a value; FALSE, otherwise.
    string  rhsAttr;        // right-hand side attribute if bRhsIsAttr = TRUE
    Value   rhsValue;       // right-hand side value if bRhsIsAttr = FALSE
};


class Iterator {
    // All the relational operators and access methods are iterators.
    public:
        virtual RC getNextTuple(void *data) = 0;
        virtual void getAttributes(vector<Attribute> &attrs) const = 0;
        virtual ~Iterator() {};
};


class TableScan : public Iterator
{
    // A wrapper inheriting Iterator over RM_ScanIterator
    public:
        RelationManager &rm;
        RM_ScanIterator *iter;
        string tableName;
        vector<Attribute> attrs;
        vector<string> attrNames;
        RID rid;

        TableScan(RelationManager &rm, const string &tableName, const char *alias = NULL):rm(rm)
        {
        	//Set members
        	this->tableName = tableName;

            // Get Attributes from RM
            rm.getAttributes(tableName, attrs);

            // Get Attribute Names from RM
            int i;
            for(i = 0; i < attrs.size(); ++i)
            {
                // convert to char *
                attrNames.push_back(attrs.at(i).name);
            }

            // Call RM scan to get an iterator
            iter = new RM_ScanIterator();
            rm.scan(tableName, "", NO_OP, NULL, attrNames, *iter);

            // Set alias
            if(alias) this->tableName = alias;
        };

        // Start a new iterator given the new compOp and value
        void setIterator()
        {
            iter->close();
            delete iter;
            iter = new RM_ScanIterator();
            rm.scan(tableName, "", NO_OP, NULL, attrNames, *iter);
        };

        RC getNextTuple(void *data)
        {
            return iter->getNextTuple(rid, data);
        };

        void getAttributes(vector<Attribute> &attrs) const
        {
            attrs.clear();
            attrs = this->attrs;
            int i;

            // For attribute in vector<Attribute>, name it as rel.attr
            for(i = 0; i < attrs.size(); ++i)
            {
                string tmp = tableName;
                tmp += ".";
                tmp += attrs.at(i).name;
                attrs.at(i).name = tmp;
            }
        };

        ~TableScan()
        {
        	iter->close();
        };
};


class IndexScan : public Iterator
{
    // A wrapper inheriting Iterator over IX_IndexScan
    public:
        RelationManager &rm;
        RM_IndexScanIterator *iter;
        string tableName;
        string attrName;
        vector<Attribute> attrs;
        char key[PAGE_SIZE];
        RID rid;

        IndexScan(RelationManager &rm, const string &tableName, const string &attrName, const char *alias = NULL):rm(rm)
        {
        	// Set members
        	this->tableName = tableName;
        	this->attrName = attrName;


            // Get Attributes from RM
            rm.getAttributes(tableName, attrs);

            // Call rm indexScan to get iterator
            iter = new RM_IndexScanIterator();
            rm.indexScan(tableName, attrName, NULL, NULL, true, true, *iter);

            // Set alias
            if(alias) this->tableName = alias;
        };

        // Start a new iterator given the new key range
        void setIterator(void* lowKey,
                         void* highKey,
                         bool lowKeyInclusive,
                         bool highKeyInclusive)
        {
            iter->close();
            delete iter;
            iter = new RM_IndexScanIterator();
            rm.indexScan(tableName, attrName, lowKey, highKey, lowKeyInclusive,
                           highKeyInclusive, *iter);
        };

        RC getNextTuple(void *data)
        {
            int rc = iter->getNextEntry(rid, key);
            if(rc == 0)
            {
                rc = rm.readTuple(tableName.c_str(), rid, data);
            }
            return rc;
        };

        void getAttributes(vector<Attribute> &attrs) const
        {
            attrs.clear();
            attrs = this->attrs;
            int i;

            // For attribute in vector<Attribute>, name it as rel.attr
            for(i = 0; i < attrs.size(); ++i)
            {
                string tmp = tableName;
                tmp += ".";
                tmp += attrs.at(i).name;
                attrs.at(i).name = tmp;
            }
        };

        ~IndexScan()
        {
            iter->close();
        };
};


class Filter : public Iterator {
    // Filter operator
    public:
		Iterator *input;
		Condition condition;
		vector<Attribute> attrs;
		void *value;

        Filter(Iterator *input,               // Iterator of input R
               const Condition &condition     // Selection condition
        );
        ~Filter(){
        		free(value);
        };

        RC getNextTuple(void *data) {
        		while(this->input->getNextTuple(data) != QE_EOF){
        			// right-hand side is an attribute
        			// need to get left value and right value and then compare
        			if(condition.bRhsIsAttr){
        				void *rightVal;
        				int offset = ceil((double)attrs.size()/CHAR_BIT);
        				int leftLen = 0;
        				int rightLen = 0;
        				AttrType attrType;
        				for(int i=0; i<attrs.size(); i++){
        					// get left value
        					if(attrs[i].name.compare(condition.lhsAttr) == 0){
							switch(attrs[i].type){
								case TypeInt: {
									value = malloc(sizeof(int));
									memset(value, 0, sizeof(int));
									memcpy((char *)value, (char *)data + offset, sizeof(int));
									offset += sizeof(int);
									break;
								}
								case TypeReal: {
									value = malloc(sizeof(float));
									memset(value, 0, sizeof(float));
									memcpy((char *)value, (char *)data + offset, sizeof(float));
									offset += sizeof(float);
									break;
								}
								case TypeVarChar: {
									memcpy(&leftLen, (char *)data + offset, sizeof(int));
									offset += sizeof(int);
									value = malloc(leftLen+1);
									memset(value, 0, leftLen+1);
									memcpy((char *)value, (char *)data + offset, leftLen);
									offset += leftLen;
									break;
								}
							}
							attrType = attrs[i].type;
							continue;
        					}
        					// get right value
        					if(attrs[i].name.compare(condition.rhsAttr) == 0){
							switch(attrs[i].type){
								case TypeInt: {
									rightVal = malloc(sizeof(int));
									memset(rightVal, 0, sizeof(int));
									memcpy((char *)rightVal, (char *)data + offset, sizeof(int));
									offset += sizeof(int);
									break;
								}
								case TypeReal: {
									rightVal = malloc(sizeof(float));
									memset(rightVal, 0, sizeof(float));
									memcpy((char *)rightVal, (char *)data + offset, sizeof(float));
									offset += sizeof(float);
									break;
								}
								case TypeVarChar: {
									memcpy(&rightLen, (char *)data + offset, sizeof(int));
									offset += sizeof(int);
									rightVal = malloc(rightLen+1);
									memset(rightVal, 0, rightLen+1);
									memcpy((char *)rightVal, (char *)data + offset, rightLen);
									offset += rightLen;
									break;
								}
							}
							continue;
        					}
        					switch(attrs[i].type){
        						case TypeInt: {
        							offset += sizeof(int);
        							break;
        						}
        						case TypeReal: {
        							offset += sizeof(float);
        							break;
        						}
        						case TypeVarChar: {
        							int varCharLen = 0;
        							memcpy(&varCharLen, (char *)data + offset, sizeof(int));
        							offset += varCharLen + sizeof(int);
        						}
        					}
        				}
        				// compare two values
    	                if(attrType!=TypeVarChar){
    	                	//compareNum(void* storedValue, const CompOp compOp, const void *valueToCompare, AttrType type);
    	                		if(RBFM_ScanIterator::compareNum(value, condition.op, rightVal, attrType)){
    	                			free(rightVal);
    	                			return 0;
    	                		}
    	                }
    	                else{
    	                	//compareVarChar(int &storedValue_len, void* storedValue, const CompOp compOp, int &valueToCompare_len, const void *valueToCompare);
    	                		if(RBFM_ScanIterator::compareVarChar(leftLen, value, condition.op, rightLen, rightVal)){
    	                			free(rightVal);
    	                			return 0;
    	                		}
    	                }
        			}
        			else{
        				// right-hand side is value
        				// read attribute from data and compare value with condition
        				int offset = ceil((double)attrs.size()/CHAR_BIT);
        				int leftLen = 0;
        				int rightLen = 0;
        				for(int i=0; i<attrs.size(); i++){
        					if(attrs[i].name.compare(condition.lhsAttr)==0){
        						switch(attrs[i].type){
        							case TypeInt: {
        								value = malloc(sizeof(int));
        								memset(value, 0, sizeof(int));
        								memcpy(value, (char *)data+offset, sizeof(int));
        								offset += sizeof(int);
                                        //cout<<"value: "<<*(int*)value<<endl;
        								break;
        							}
        							case TypeReal: {
        								value = malloc(sizeof(float));
        								memset(value, 0, sizeof(float));
        								memcpy(value, (char *)data+offset, sizeof(float));
        								offset += sizeof(float);
                                        //cout<<"value: "<<*(int*)value<<endl;
        								break;
        							}
        							case TypeVarChar: {
        								memcpy(&leftLen, (char *)data+offset, sizeof(int));
                                        offset+=sizeof(int);
                                        //cout<<"rightLen: "<<rightLen<<endl;
        								value = malloc(leftLen+1);
        								memset(value, 0, leftLen+1);
        								memcpy((char*)value, (char *)data+offset, leftLen);
        								offset += leftLen;
        								break;
        							}
        						}
        						if(attrs[i].type!=TypeVarChar){
        							if(RBFM_ScanIterator::compareNum(value, condition.op, condition.rhsValue.data, attrs[i].type)){
        								return 0;
        							}
        						}
        						else{
        							memcpy(&rightLen, (char *)condition.rhsValue.data, sizeof(int));
                                    char* rightVal=new char[rightLen];
                                    memcpy(rightVal,(char *)condition.rhsValue.data+sizeof(int), rightLen);
        							if(RBFM_ScanIterator::compareVarChar(leftLen, value, condition.op, rightLen, rightVal)){
        								return 0;
        							}
        						}
        					}
        					if(attrs[i].type != TypeVarChar){
        						offset += sizeof(int);
        					}
        					else{
        						int varCharLen = 0;
        						memcpy(&varCharLen, (char *)data+offset, sizeof(int));
        						offset += varCharLen + sizeof(int);
        					}
        				}
        			}
        		}
        		return QE_EOF;
        };
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const{
        		this->input->getAttributes(attrs);
        };
};


class Project : public Iterator {
    // Projection operator
    public:
		Iterator *input;
		vector<string> attrNames;
		vector<Attribute> allAttrs;
		vector<Attribute> projectAttrs;
		void *buffer;
        Project(Iterator *input,                    // Iterator of input R
              const vector<string> &attrNames);   // vector containing attribute names
        ~Project(){};

        RC getNextTuple(void *data) {
        		while(input->getNextTuple(buffer)!=QE_EOF){
        			// read all record into buffer, then copy the project attrs into data
                    int aNullIndicatorSize = ceil((double)allAttrs.size()/CHAR_BIT);
                    int pNullIndicatorSize = ceil((double)projectAttrs.size()/CHAR_BIT);
        			int allOffset = aNullIndicatorSize;
        			int projectOffset = pNullIndicatorSize;
        			for(int i=0; i<allAttrs.size(); i++){
        				bool notFound = true;
        				for(int j=0; j<projectAttrs.size(); j++){
        					if(allAttrs[i].name.compare(projectAttrs[j].name)==0){
        						if(projectAttrs[j].type != TypeVarChar){
        							memcpy((char*)data+projectOffset, (char*)buffer+allOffset, sizeof(int));
        							allOffset += sizeof(int);
        							projectOffset += sizeof(int);
        						}
        						else{
        							int varCharLen = 0;
        							memcpy(&varCharLen, (char*)buffer+allOffset, sizeof(int));
        							memcpy((char*)data+projectOffset, (char*)buffer+allOffset, varCharLen+sizeof(int));
        							allOffset += varCharLen + sizeof(int);
        							projectOffset += varCharLen + sizeof(int);
        						}
        						notFound = false;
        						break;
        					}
        				}
        				if(notFound){
        					if(allAttrs[i].type != TypeVarChar){
        						allOffset += sizeof(int);
        					}
        					else{
        						int varCharLen = 0;
        						memcpy(&varCharLen, (char*)buffer+allOffset, sizeof(int));
        						allOffset += varCharLen + sizeof(int);
        					}
        				}
        			}
        			return 0;
        		}
        		return QE_EOF;
        };
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const{
        		attrs.clear();
        		for(int i=0; i<attrNames.size(); i++){
        			for(int j=0; j<allAttrs.size(); j++){
        				if(allAttrs[j].name.compare(attrNames[i])==0){
        					attrs.push_back(allAttrs[j]);
        					break;
        				}
        			}
        		}
        };
};

class BNLJoin : public Iterator {
    // Block nested-loop join operator
    public:
    Iterator *left;
    TableScan *right;

    vector<Attribute> leftAttrs;
    vector<Attribute> rightAttrs;

    Condition condition;
    AttrType attrType;

    int lCondAttrIndex;
    int rCondAttrIndex;

    int bufferSize;

    void* blockData;
    map<Key, vector<Pair> > blockMap;

    int maxRecrodLength;

    int currentIndex;

    BNLJoin(Iterator *leftIn,            // Iterator of input R
               TableScan *rightIn,           // TableScan Iterator of input S
               const Condition &condition,   // Join condition
               const unsigned numPages       // # of pages that can be loaded into memory,
			                                 //   i.e., memory block size (decided by the optimizer)
    );
    ~BNLJoin();

    RC getNextTuple(void *data){
        
        if(this->blockMap.size() ==0)
            return -1;
        else{
            map<Key, vector<Pair> >::iterator it = blockMap.begin();
            
            Key currentKey = it->first;
            Pair currentPair = (it->second)[currentIndex];
            
            char* rightData = new char[PAGE_SIZE];
            
            while(right->getNextTuple(rightData)==0){
                char* rightAttrValue = new char[PAGE_SIZE];
                if(this->rCondAttrIndex != -1){
                    int NullIndicatorSize = (double)ceil(rightAttrs.size() / 8.0);
                    
                    int offset = NullIndicatorSize;
                    int nullIndicatorOffset = 0;
                    int short standardNullIndicator = 0x80;
                    char nullIndicator = 0;
                    
                    for (int i = 0; i < rCondAttrIndex; i++) {
                        if (i % 8 == 0) {
                            memcpy(&nullIndicator, (char*) rightData + nullIndicatorOffset, NullIndicatorSize);
                            nullIndicatorOffset++;
                            standardNullIndicator = 0x80;
                        }
                        
                        if ((nullIndicator & standardNullIndicator) == 0) {
                            switch(rightAttrs[i].type){
                                case TypeInt:
                                    offset += sizeof(int);
                                    break;
                                case TypeReal:
                                    offset += sizeof(float);
                                    break;
                                case TypeVarChar:
                                    int varLength;
                                    memcpy(&varLength,(char*) rightData + offset,sizeof(int));
                                    offset += varLength;
                                    offset += sizeof(int);
                                    break;
                            }
                        }
                        
                        standardNullIndicator = standardNullIndicator >> 1;
                    }
                    switch(rightAttrs[rCondAttrIndex].type){
                        case TypeInt:
                            memcpy(rightAttrValue, (char*) rightData + offset, sizeof(int));
                            break;
                        case TypeReal:
                            memcpy(rightAttrValue, (char*) rightData + offset, sizeof(float));
                            break;
                        case TypeVarChar:
                            int varLength;
                            memcpy(&varLength,(char*) rightData + offset,sizeof(int));
                            memcpy(rightAttrValue, (char*) rightData + offset, sizeof(int) + varLength);
                            break;
                            
                    }
                }
                if(this->attrType!=TypeVarChar){
                    if(RBFM_ScanIterator::compareNum(currentKey.data,this->condition.op,rightAttrValue,this->attrType)){
                        char* leftData = new char[PAGE_SIZE];
                        memcpy(leftData,(char*)this->blockData+currentPair.offset,currentPair.length);
                        Pair::combineData(leftData, this->leftAttrs, rightData, this->rightAttrs, (char*)data);
                        delete[] leftData;
                        delete[] rightAttrValue;
                        delete[] rightData;
                        return 0;
                    }
                    else{
                        delete[] rightAttrValue;
                    }
                }
                else{
                    int leftLen;
                    memcpy(&leftLen,currentKey.data,sizeof(int));
                    int rightLen;
                    memcpy(&rightLen,rightAttrValue,sizeof(int));
                    if(RBFM_ScanIterator::compareVarChar(leftLen, (char*)currentKey.data+sizeof(int), condition.op, rightLen, (char*)rightAttrValue+sizeof(int))){
                        char* leftData = new char[PAGE_SIZE];
                        memcpy(leftData,(char*)this->blockData+currentPair.offset,currentPair.length);
                        Pair::combineData(leftData, this->leftAttrs, rightData, this->rightAttrs, (char*)data);
                        delete[] leftData;
                        delete[] rightAttrValue;
                        delete[] rightData;
                        return 0;
                    }
                    else{
                        delete[] rightAttrValue;
                    }
                }
            }
            
            TableScan* right= new TableScan(this->right->rm, this->right->tableName);
            //delete this->right;
            this->right = right;
            
            if(currentIndex<it->second.size()-1){
                currentIndex++;
                return getNextTuple(data);
            }else{
                this->blockMap.erase(it->first);
                if(this->blockMap.size() ==0){
                    this->loadBlock();
                    currentIndex=0;
                }
                return getNextTuple(data);
            }
        }
        
    };
    
    int getAttrIndex(vector<Attribute> attrs, string attrName){
        int index;
        for(index=0; index < attrs.size(); index++) {
            if (strcmp(attrName.c_str(), attrs[index].name.c_str()) == 0)
                break;
        }
        return index;
    }
    // For attribute in vector<Attribute>, name it as rel.attr
    void getAttributes(vector<Attribute> &attrs) const;

    RC loadBlock(){
        if(blockData)
            free(blockData);
        if(!blockMap.empty())
            blockMap.clear();
        blockData = malloc(bufferSize);
        
        void* data = malloc(PAGE_SIZE);
        int offset = 0;
        
        while ((bufferSize-maxRecrodLength)>offset && left->getNextTuple(data)==0) {
            RecordBasedFileManager *rbf_manager=RecordBasedFileManager::instance();
            int recordLength = rbf_manager->getRecordSize(this->leftAttrs,data);
            
            memcpy((char*)blockData+offset, data, recordLength);
            void* page = malloc(PAGE_SIZE);
            
            int NullIndicatorSize = (double)ceil(this->leftAttrs.size() / 8.0);
            int valOffset = NullIndicatorSize;
            int nullIndicatorOffset = 0;
            int short standardNullIndicator = 0x80;
            char nullIndicator = 0;
            
            for (int i = 0; i < this->lCondAttrIndex; i++) {
                if (i % 8 == 0) {
                    memcpy(&nullIndicator, (char*) data + nullIndicatorOffset, NullIndicatorSize);
                    nullIndicatorOffset++;
                    standardNullIndicator = 0x80;
                }
                
                if ((nullIndicator & standardNullIndicator) == 0) {
                    switch(this->leftAttrs[i].type){
                        case TypeInt:
                            valOffset += sizeof(int);
                            break;
                        case TypeReal:
                            valOffset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int varLength;
                            memcpy(&varLength,(char*) data + valOffset,sizeof(int));
                            valOffset += varLength;
                            valOffset += sizeof(int);
                            break;
                    }
                }
                
                standardNullIndicator = standardNullIndicator >> 1;
            }
            switch(this->leftAttrs[this->lCondAttrIndex].type){
                case TypeInt:
                    memcpy(page, (char*) data + valOffset, sizeof(int));
                    break;
                case TypeReal:
                    memcpy(page, (char*) data + valOffset, sizeof(float));
                    break;
                case TypeVarChar:
                    int varLength;
                    memcpy(&varLength,(char*) data + offset,sizeof(int));
                    memcpy(page, (char*) data + valOffset, sizeof(int) + varLength);
                    break;
                    
            }
            
            Key* key = new Key(this->attrType, page);
            vector<Pair> container = blockMap[*key];
            Pair pair(offset,recordLength);
            offset += recordLength;
            
            if (!container.empty()) {
                container.push_back(pair);
            } else {
                vector<Pair> pairs;
                pairs.push_back(pair);
                blockMap[*key] = pairs;
            }
            
            delete key;
            free(page);
        }
        //currentblockMapIndex = 0;
        currentIndex = 0;
        return 0;
    };
};


class INLJoin : public Iterator {
    // Index nested-loop join operator
    public:
		Iterator *leftIn;
		IndexScan *rightIn;
		Condition condition;
		vector<Attribute> leftAttrs;
		vector<Attribute> rightAttrs;
        void *lData;
        void *rData;
		void *lVal;
		void *rVal;
        bool sameRecord;

        INLJoin(Iterator *leftIn,           // Iterator of input R
               IndexScan *rightIn,          // IndexScan Iterator of input S
               const Condition &condition   // Join condition
        );
        ~INLJoin(){
        		free(lVal);
        		free(rVal);
        };

        RC getNextTuple(void *data){
            int offset = 0;
            int leftLen = 0;
            int rightLen = 0;
            lData=malloc(PAGE_SIZE);
            memset(lData, 0, PAGE_SIZE);
            while (leftIn->getNextTuple(lData) != QE_EOF) {
                int lNullIndicatorSize = ceil((double)leftAttrs.size()/CHAR_BIT);
                char lNullIndicator;
                memcpy(&lNullIndicator,(char *)lData + offset,lNullIndicatorSize);
                offset+=lNullIndicatorSize;

                for (int i=0; i<leftAttrs.size(); ++i) {
                    if (leftAttrs[i].name.compare(condition.lhsAttr) == 0) {
                        if (leftAttrs[i].type != TypeVarChar) {
                            lVal = malloc(sizeof(int));
                            memset(lVal, 0, sizeof(int));
                            memcpy(lVal, (char *)lData + offset, sizeof(int));
                        }
                        else {
                            memcpy(&leftLen, (char *)lData + offset, sizeof(int));
                            lVal = malloc(leftLen+1);
                            memset(lVal, 0, leftLen+1);
                            memcpy(lVal, (char *)lData + offset + sizeof(int), leftLen);
                        }
                        break;
                    }
                    if (leftAttrs[i].type != TypeVarChar) {
                        offset += 4;
                    }
                    else {
                        int len = 0;
                        memcpy(&len, (char *)lData + offset, sizeof(int));
                        offset += len + 4;
                    }
                }
            outerRecord:
                rData=malloc(PAGE_SIZE);
                memset(rData, 0, PAGE_SIZE);
                while (rightIn->getNextTuple(rData) != QE_EOF) {
                    int rNullIndicatorSize = ceil((double)rightAttrs.size()/CHAR_BIT);
                    char rNullIndicator;
                    memcpy(&rNullIndicator,(char *)rData + offset,rNullIndicatorSize);
                    offset=rNullIndicatorSize;
                    int i;
                    for (i=0; i<rightAttrs.size(); ++i) {
                        if (rightAttrs[i].name.compare(condition.rhsAttr) == 0) {
                            if (rightAttrs[i].type != TypeVarChar) {
                                rVal = malloc(sizeof(int));
                                memset(rVal, 0, sizeof(int));
                                memcpy(rVal, (char *)rData + offset, sizeof(int));
                                //cout << "rVal: " << *(float*)rVal << endl;
                            }
                            else {
                                memcpy(&rightLen, (char *)rData + offset, sizeof(int));
                                rVal = malloc(rightLen+1);
                                memset(rVal, 0, rightLen+1);
                                memcpy(rVal, (char *)rData + offset + sizeof(int), rightLen);
                            }
                            break;
                        }
                        if (rightAttrs[i].type != TypeVarChar) {
                            offset += 4;
                        }
                        else {
                            int len = 0;
                            memcpy(&len, (char *)rData + offset, sizeof(int));
                            offset += len + 4;
                        }
                    }
                    // compare right value with left value, if true, combine them into data
                    // debug info
                    //cout << "INLJoin compare info " << endl;

                    if(rightAttrs[i].type != TypeVarChar){
                        //cout<<"lVal: "<<*(float*)lVal<<" rVal: "<<*(float*)rVal<<endl;
                    		if(RBFM_ScanIterator::compareNum(lVal, condition.op, rVal, rightAttrs[i].type)){
                    			Pair::combineData((char*)lData, leftAttrs, (char*)rData, rightAttrs, (char*)data);
                                 sameRecord=true;
                            free(lData);
                            free(rData);
                            return 0;
                    		}
                    }
                    else{
                    		if(RBFM_ScanIterator::compareVarChar(leftLen, lVal, condition.op, rightLen, rVal)){
                            Pair::combineData((char*)lData, leftAttrs, (char*)rData, rightAttrs, (char*)data);
                                 sameRecord=true;
                            free(lData);
                            free(rData);
                            return 0;
                    		}
                    }
                }
                //free(rData);
                IndexScan* innerTmp = new IndexScan(rightIn->rm,rightIn->tableName,rightIn->attrName);
                //delete this->rightIn;
                rightIn = innerTmp;
                
                memset(rData, 0, PAGE_SIZE);
                if(sameRecord){
                    sameRecord=false;
                    goto outerRecord;
                }else{
                    return QE_EOF;
                }
            }
            return QE_EOF;
        };
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const{
            for(int i=0; i< this->leftAttrs.size(); i++)
                attrs.push_back(this->leftAttrs[i]);
            for(int i=0; i< this->rightAttrs.size(); i++)
                attrs.push_back(this->rightAttrs[i]);
        };
};

// Optional for everyone. 10 extra-credit points
class GHJoin : public Iterator {
    // Grace hash join operator
    public:
      GHJoin(Iterator *leftIn,               // Iterator of input R
            Iterator *rightIn,               // Iterator of input S
            const Condition &condition,      // Join condition (CompOp is always EQ)
            const int numPartitions     // # of partitions for each relation (decided by the optimizer)
      ){};
      ~GHJoin(){};

      RC getNextTuple(void *data){return QE_EOF;};
      // For attribute in vector<Attribute>, name it as rel.attr
      void getAttributes(vector<Attribute> &attrs) const{};
};

class AggregateData{
public:
    float realValue;
	int intValue;
	int count;
    AttrType attrType;

	void init(AttrType type, AggregateOp op){
        count = 0;
        attrType = type;
        switch (op){
            case MIN:
                realValue = INT_MAX;
                intValue  = INT_MAX;
                break;
            case MAX:
                realValue = INT_MIN;
                intValue = INT_MIN;
                break;
            case SUM:
                realValue = 0.0;
                intValue = 0;
                break;
            case AVG:
                realValue = 0.0;
                intValue = 0;
                break;
            case COUNT:
                realValue = 0.0;
                intValue = 0;
                break;
        }
    }

	void write(AggregateOp op, void* dest, char nullIndicator, int nullIndicatorSize) const{
        if (op == AVG){
        	    memcpy(dest, &nullIndicator, sizeof(nullIndicator));
            memcpy((char*)dest+nullIndicatorSize, &realValue, sizeof(float));
        }
        else{
        		memcpy(dest, &nullIndicator, sizeof(nullIndicator));
        	    memcpy((char*)dest+nullIndicatorSize, &realValue, sizeof(float));
        }
    }

	void append(AggregateOp op, float realData, int intData){
        count += 1;
        switch (op){
            case MIN:
                realValue = (realData < realValue) ? realData : realValue;
                intValue = (intData  < intValue) ? intData : intValue;
                break;
            case MAX:
                realValue = (realData > realValue) ? realData : realValue;
                intValue = (intData  > intValue) ? intData : intValue;
                //cout << "realValue: " << realValue << endl;
                break;
            case SUM:
                realValue += realData;
                intValue += intData;
                break;
            case AVG:
                realValue = ((realValue * (count-1)) + realData) / count;
                intValue = (int)((realValue * (count-1)) + realData) / count;
                break;
            case COUNT:
                realValue = (float)count;
                intValue = count;
                break;
        }
    }
};

class Aggregate : public Iterator {
// Aggregation operator
public:
    Iterator *input;
    Attribute aggAttr;
    AggregateOp op;
    AggregateData aggData;
    vector<Attribute> attrs;

    Aggregate(Iterator *input,                              // Iterator of input R
              Attribute aggAttr,                            // The attribute over which we are computing an aggregate
              AggregateOp op                                // Aggregate operation
    );

    // Extra Credit
    Aggregate(Iterator *input,                              // Iterator of input R
              Attribute aggAttr,                            // The attribute over which we are computing an aggregate
              Attribute gAttr,                              // The attribute over which we are grouping the tuples
              AggregateOp op                                // Aggregate operation
    );

    ~Aggregate(){};

    RC getNextTuple(void *data){
    		void *buffer = malloc(PAGE_SIZE);
        memset(buffer, 0, sizeof(PAGE_SIZE));
        int nullIndicatorSize = (double)ceil(attrs.size() / 8.0);
        char nullIndicator;
        memcpy(&nullIndicator,(char *)data,nullIndicatorSize);
        RC rc = QE_EOF;
        rc = input->getNextTuple(buffer);
        if(rc==QE_EOF){
        		return rc;
        }
        while(rc != QE_EOF){
            int offset = nullIndicatorSize;
            for (int i = 0; i < attrs.size(); i++){
                if (attrs[i].name.compare(aggAttr.name) == 0) break;
                if (attrs[i].type == TypeVarChar)
                    offset += sizeof(int) + (*(int *)buffer + offset);
                else
                    offset += sizeof(int);
            }
            int value = *(int*)((char*)buffer + offset);
            //cout << "value: " << value << endl;
            aggData.append(op, (float)value, value);
            aggData.write(op, data, nullIndicator, nullIndicatorSize);
            rc = input->getNextTuple(buffer);
        }
        //cout << "aggData.realValue: " << aggData.realValue << endl;
        //aggData.write(op, data, nullIndicator, nullIndicatorSize);
        	return 0;
    };

    // Please name the output attribute as aggregateOp(aggAttr)
    // E.g. Relation=rel, attribute=attr, aggregateOp=MAX
    // output attrname = "MAX(rel.attr)"
    void getAttributes(vector<Attribute> &attrs) const{
			Attribute attr;
			attr.type = this->aggAttr.type;
			attr.length = this->aggAttr.length;

			if(this->op == MAX){
				attr.name = "MAX("+this->aggAttr.name+")";
			}else if(this->op == MIN){
				attr.name = "MIN("+this->aggAttr.name+")";
			}else if(this->op == AVG){
				attr.name = "AVG("+this->aggAttr.name+")";
			}else if(this->op == SUM){
				attr.name = "SUM("+this->aggAttr.name+")";
			}else if(this->op == COUNT){
				attr.name = "COUNT("+this->aggAttr.name+")";
			}
			attrs.push_back(attr);
    };
};

#endif
