#include <osg/PagedLOD>

using namespace osg;

PagedLOD::PerRangeData::PerRangeData():
    _priorityOffset(0.0f),
    _priorityScale(0.0f),
    _timeStamp(0.0f) {}

PagedLOD::PerRangeData::PerRangeData(const PerRangeData& prd):
    _filename(prd._filename),
    _priorityOffset(prd._priorityOffset),
    _priorityScale(prd._priorityScale),
    _timeStamp(prd._timeStamp) {}

PagedLOD::PerRangeData& PagedLOD::PerRangeData::operator = (const PerRangeData& prd)
{
    if (this==&prd) return *this;
    _filename = prd._filename;
    _priorityOffset = prd._priorityOffset;
    _priorityScale = prd._priorityScale;
    _timeStamp = prd._timeStamp;
    return *this;
}

PagedLOD::PagedLOD()
{
    _centerMode = USER_DEFINED_CENTER;
    _radius = -1;
    _numChildrenThatCannotBeExpired = 0;
}

PagedLOD::PagedLOD(const PagedLOD& plod,const CopyOp& copyop):
    LOD(plod,copyop),
    _radius(plod._radius),
    _numChildrenThatCannotBeExpired(plod._numChildrenThatCannotBeExpired),
    _perRangeDataList(plod._perRangeDataList)
{
}


void PagedLOD::traverse(NodeVisitor& nv)
{

    double timeStamp = nv.getFrameStamp()?nv.getFrameStamp()->getReferenceTime():0.0;
    bool updateTimeStamp = nv.getVisitorType()==osg::NodeVisitor::CULL_VISITOR;

    switch(nv.getTraversalMode())
    {
        case(NodeVisitor::TRAVERSE_ALL_CHILDREN):
            std::for_each(_children.begin(),_children.end(),NodeAcceptOp(nv));
            break;
        case(NodeVisitor::TRAVERSE_ACTIVE_CHILDREN):
        {
            float distance = nv.getDistanceToEyePoint(getCenter(),true);

            int lastChildTraversed = -1;
            bool needToLoadChild = false;
            for(unsigned int i=0;i<_rangeList.size();++i)
            {    
                if (_rangeList[i].first<=distance && distance<_rangeList[i].second)
                {
                    if (i<_children.size())
                    {
                        if (updateTimeStamp) _perRangeDataList[i]._timeStamp=timeStamp;

                        _children[i]->accept(nv);
                        lastChildTraversed = (int)i;
                    }
                    else
                    {
                        needToLoadChild = true;
                    }
                }
            }
            
            if (needToLoadChild)
            {
                unsigned int numChildren = _children.size();
                
                // select the last valid child.
                if (numChildren>0 && ((int)numChildren-1)!=lastChildTraversed)
                {
                    if (updateTimeStamp) _perRangeDataList[numChildren-1]._timeStamp=timeStamp;
                    _children[numChildren-1]->accept(nv);
                }
                
                // now request the loading of the next unload child.
                if (nv.getDatabaseRequestHandler() && numChildren<_perRangeDataList.size())
                {
                    // compute priority from where abouts in the required range the distance falls.
                    float priority = (_rangeList[numChildren].second-distance)/(_rangeList[numChildren].second-_rangeList[numChildren].first);
                    
                    // modify the priority according to the child's priority offset and scale.
                    priority = _perRangeDataList[numChildren]._priorityOffset + priority * _perRangeDataList[numChildren]._priorityScale;

                    nv.getDatabaseRequestHandler()->requestNodeFile(_perRangeDataList[numChildren]._filename,this,priority,nv.getFrameStamp());
                }
                
                
            }
            
            
           break;
        }
        default:
            break;
    }
}

bool PagedLOD::computeBound() const
{
    if (_centerMode==USER_DEFINED_CENTER && _radius>=0.0f)
    {
        _bsphere._center = _userDefinedCenter;
        _bsphere._radius = _radius;
        _bsphere_computed = true;

        return true;
    }
    else
    {
        return LOD::computeBound();
    }
}

void PagedLOD::childRemoved(unsigned int pos, unsigned int numChildrenToRemove)
{
    LOD::childRemoved(pos, numChildrenToRemove);
}

void PagedLOD::childInserted(unsigned int pos)
{
    LOD::childInserted(pos);
}

void PagedLOD::rangeRemoved(unsigned int pos, unsigned int numChildrenToRemove)
{
    LOD::rangeRemoved(pos, numChildrenToRemove);
}

void PagedLOD::rangeInserted(unsigned int pos)
{
    LOD::rangeInserted(pos);
    expandPerRangeDataTo(pos);
}

void PagedLOD::expandPerRangeDataTo(unsigned int pos)
{
    if (pos>=_perRangeDataList.size()) _perRangeDataList.resize(pos+1);
}

bool PagedLOD::addChild( Node *child )
{
    if (LOD::addChild(child))
    {
        expandPerRangeDataTo(_children.size());
        return true;
    }
    return false;
}

bool PagedLOD::addChild(Node *child, float min, float max)
{
    if (LOD::addChild(child,min,max))
    {
        expandPerRangeDataTo(_children.size());
        return true;
    }
    return false;
}


bool PagedLOD::addChild(Node *child, float min, float max,const std::string& filename, float priorityOffset, float priorityScale)
{
    if (LOD::addChild(child,min,max))
    {
        setFileName(_children.size()-1,filename);
        setPriorityOffset(_children.size()-1,priorityOffset);
        setPriorityScale(_children.size()-1,priorityScale);
        return true;
    }
    return false;
}

bool PagedLOD::removeChild( Node *child )
{
    // find the child's position.
    unsigned int pos=getChildIndex(child);
    if (pos==_children.size()) return false;
    
    if (pos<_rangeList.size()) _rangeList.erase(_rangeList.begin()+pos);
    if (pos<_perRangeDataList.size()) _perRangeDataList.erase(_perRangeDataList.begin()+pos);
    
    return Group::removeChild(child);    
}

void PagedLOD::removeExpiredChildren(double expiryTime,NodeList& removedChildren)
{
    if (_children.size()>_numChildrenThatCannotBeExpired)
    {
        if (!_perRangeDataList[_children.size()-1]._filename.empty() && _perRangeDataList[_children.size()-1]._timeStamp<expiryTime)
        {
            //removedChildren.push_back(_children[_children.size()-1].get());
            Group::removeChild(_children[_children.size()-1].get());
        }
    }
}
