/*
 * Copyright (c) 2025 Huawei Device, Inc. Ltd. and <马弓手>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "XMLParser.h"
#include <cstdlib>
#include <cstring>

SXMLNode::~SXMLNode()
{
    freeChildElements();
    freeChildAttrs();
}

void SXMLNode::freeChildElements()
{
    SXMLDocElement* child = m_pElements;
    while (child) {
        SXMLDocElement* tmp = child->m_next;
        delete child;
        child = tmp;
    }
    m_pElements = nullptr;
}

void SXMLNode::freeChildAttrs()
{
    SXMLAttr* attr = m_pAttrs;
    while (attr) {
        SXMLAttr* tmp = attr->m_pNext;
        delete attr;
        attr = tmp;
    }
    m_pAttrs = nullptr;
}

bool SXMLNode::addNode(SXMLNode* node)
{
    return addElement(node);
}

bool SXMLNode::addElement(SXMLDocElement* element)
{
    SXMLDocElement* child = m_pElements;
    if (child == nullptr) {
        m_pElements = element;
        return true;
    }
    while (child->m_next) {
        child = child->m_next;
    }
    child->m_next = element;
    return true;
}

bool SXMLNode::addAttr(SXMLAttr* attr)
{
    SXMLAttr* a = m_pAttrs;
    if (a == nullptr) {
        m_pAttrs = attr;
        return true;
    }
    while (a->m_pNext) {
        a = a->m_pNext;
    }
    a->m_pNext = attr;
    return true;
}

int SXMLNode::getNodesCount()
{
    return getElementsCount(XML_ELM_NODE);
}

int SXMLNode::getElementsCount(SXMLDocElementType type)
{
    int ret = 0;
    SXMLDocElement* child = m_pElements;
    while (child) {
        if (type != XML_ELM_UNKNOWN && type == child->m_type) {
            ret++;
        }
        child = child->m_next;
    }
    return ret;
}

int SXMLNode::getAttrsCount()
{
    int ret = 0;
    SXMLAttr* attr = m_pAttrs;
    while (attr) {
        ret++;
        attr = attr->m_pNext;
    }
    return ret;
}

SXMLNode* SXMLNode::getNodeStep(const char* name)
{
    SXMLDocElement* child = m_pElements;
    while (child) {
        if (child->m_type == XML_ELM_NODE) {
            if ((static_cast<SXMLNode*>(child))->m_strName == name) {
                return static_cast<SXMLNode*>(child);
            }
        }
        child = child->m_next;
    }
    return nullptr;
}

SXMLNode* SXMLNode::getNode(const char* path)
{
    SXMLNode* node = this;
    char* str = strdup(path);
    char *p, *offset = str;
    if (*offset == '/') { // don't use path with '/' start.
        free(str);
        return nullptr;
    }

    while (true) {
        if (strlen(offset) == 0) {
            break;
        }
        p = strchr(offset, '/');
        if (p != nullptr) {
            p[0] = 0;
            node = node->getNodeStep(offset);
            offset = p + 1;
        } else {
            node = node->getNodeStep(offset);
            break;
        }

        if (node == nullptr) {
            free(str);
            return nullptr;
        }
    }
    free(str);
    return node;
}

SXMLDocElement* SXMLNode::getDocElement(SXMLDocElementType type, int nIndex)
{
    SXMLDocElement* child = m_pElements;
    int pos = 0;

    while (child) {
        if (type != XML_ELM_UNKNOWN && type == child->m_type) {
            if (pos == nIndex) {
                return child;
            }
            pos++;
        }
        child = child->m_next;
    }
    return nullptr;
}

SXMLNode* SXMLNode::getNode(int nIndex)
{
    return static_cast<SXMLNode*>(getDocElement(XML_ELM_NODE, nIndex));
}

bool SXMLNode::getContentText(std::string& text, int nIndex)
{
    SXMLDocument* document = static_cast<SXMLDocument*>(getDocElement(XML_ELM_CONTENT, nIndex));
    if (document == nullptr) {
        return false;
    }
    text = document->m_strContent;
    return true;
}

std::string SXMLNode::getContentText(int nIndex)
{
    SXMLDocument* document = static_cast<SXMLDocument*>(getDocElement(XML_ELM_CONTENT, nIndex));
    if (document == nullptr) {
        return "";
    }
    return document->m_strContent;
}

SXMLDocument* SXMLNode::getContent(int nIndex)
{
    return static_cast<SXMLDocument*>(getDocElement(XML_ELM_CONTENT, nIndex));
}

SXMLNode::SXMLAttr* SXMLNode::getAttr(const char* name)
{
    SXMLAttr* attr = m_pAttrs;
    while (attr) {
        if (attr->m_strName == name) {
            return attr;
        }
        attr = attr->m_pNext;
    }
    return nullptr;
}

SXMLNode::SXMLAttr* SXMLNode::getAttr(int nIndex)
{
    SXMLAttr* attr = m_pAttrs;
    int i = 0;
    while (attr) {
        if (i == nIndex) {
            return attr;
        }
        attr = attr->m_pNext;
        i++;
    }
    return nullptr;
}

bool SXMLNode::getAttrValue(const char* name, std::string& value)
{
    SXMLAttr* attr = getAttr(name);
    if (attr == nullptr) {
        return false;
    }
    value = attr->m_strValue;
    return true;
}

std::string SXMLNode::getRawContent()
{
    int i;
    std::string ret = "<";
    ret = ret + m_strName;
    SXMLAttr* attr = m_pAttrs;
    while (attr) {
        ret = ret + " " + attr->m_strName + "=\"" + attr->m_strValue + "\"";
        attr = attr->m_pNext;
    }
    ret = ret + ">";

    for (i = 0; i < getElementsCount(XML_ELM_CONTENT); i++) {
        ret = ret + getContentText(i);
    }

    SXMLDocElement* nd = m_pElements;
    while (nd) {
        if (nd->m_type == XML_ELM_NODE) {
            ret = ret + (static_cast<SXMLNode*>(nd))->getRawContent();
        }
        nd = nd->m_next;
    }
    ret = ret + "</" + m_strName + ">\n";
    return ret;
}
