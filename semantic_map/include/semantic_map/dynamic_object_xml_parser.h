#ifndef __DYNAMIC_OBJECT_XML_PARSER__H
#define __DYNAMIC_OBJECT_XML_PARSER__H

#include <QFile>
#include <QDir>
#include <QXmlStreamWriter>
#include <QDebug>

#include <fstream>
#include "semantic_map/dynamic_object.h"


class DynamicObjectXMLParser {
public:

    typedef pcl::PointXYZRGB PointType;
    typedef pcl::PointCloud<PointType> Cloud;
    typedef typename Cloud::Ptr CloudPtr;


    DynamicObjectXMLParser(std::string rootFolder = "", bool verbose = false);
    ~DynamicObjectXMLParser();

    std::string saveAsXML(DynamicObject::Ptr object, std::string filename, std::string cloud_filename);
    DynamicObject::Ptr loadFromXML(std::string filename);

    std::string m_rootFolderPath;
    bool m_verbose;

};

#endif // __DYNAMIC_OBJECT_XML_PARSER__H
