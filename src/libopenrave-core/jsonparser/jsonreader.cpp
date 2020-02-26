// -*- coding: utf-8 -*-
// Copyright (C) 2013 Rosen Diankov <rosen.diankov@gmail.com>
//
// This file is part of OpenRAVE.
// OpenRAVE is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "jsoncommon.h"

#include <openrave/openravejson.h>
#include <rapidjson/istreamwrapper.h>
#include <string>
#include <fstream>

namespace OpenRAVE {

    class JSONReader {

    public:

        JSONReader(const AttributesList& atts, EnvironmentBasePtr penv): _penv(penv), _prefix("")
        {
            FOREACHC(itatt, atts) {
                if (itatt->first == "prefix") {
                    _prefix = itatt->second;
                }
                else if (itatt->first == "openravescheme")
                {
                    std::stringstream ss(itatt->second);
                    _vOpenRAVESchemeAliases = std::vector<std::string>((istream_iterator<std::string>(ss)), istream_iterator<std::string>());
                }
            }
            if (_vOpenRAVESchemeAliases.size() == 0) {
                _vOpenRAVESchemeAliases.push_back("openrave");
            }
            // set global scale when initalize jsonreader.

            _fGlobalScale = 1.0 / _penv->GetUnit().second;
        }

        virtual ~JSONReader()
        {
        }

        bool InitFromFile(const std::string& filename)
        {
            _filename = filename;
            _doc = _OpenDocument(_filename);
            return true;
        }

        bool InitFromURI(const std::string& uri)
        {
            _uri = uri;
            _filename = _ResolveURI(_uri);
            _doc = _OpenDocument(_filename);
            return true;
        }

        bool InitFromData(const std::string& data)
        {
            _doc = _ParseDocument(data);
            return true;
        }

        bool Extract()
        {
            std::pair<std::string, dReal> unit;
            LoadJsonValueByKey(*_doc, "unit", unit);
            _penv->SetUnit(unit);

            if (_doc->HasMember("bodies") && (*_doc)["bodies"].IsArray()) {
                for (rapidjson::Value::ValueIterator itr = (*_doc)["bodies"].Begin(); itr != (*_doc)["bodies"].End(); ++itr) {
                    _FillBody(*itr, _doc->GetAllocator());

                    bool isRobot = false;
                    LoadJsonValueByKey(*itr, "isRobot", isRobot);

                    if (isRobot) {
                        _ExtractRobot(*itr);
                    } else {
                        _ExtractKinBody(*itr);
                    }
                }
            }
            return true;
        }

        bool Extract(KinBodyPtr& ppbody) {
            // extract the first articulated system found.
            std::pair<std::string, dReal> unit;
            LoadJsonValueByKey(*_doc, "unit", unit);
            _penv->SetUnit(unit);

            if (_doc->HasMember("bodies") && (*_doc)["bodies"].IsArray()){
                rapidjson::Value::ValueIterator itr = (*_doc)["bodies"].Begin();
                _FillBody(*itr, _doc->GetAllocator()); 
                _ExtractKinBody(*itr, ppbody);
            }

            return true;
        }

        bool Extract(RobotBasePtr& pprobot){
            // extract the first robot 
            std::pair<std::string, dReal> unit;
            LoadJsonValueByKey(*_doc, "unit", unit);

            _penv->SetUnit(unit);

            for (rapidjson::Value::ValueIterator itr = (*_doc)["bodies"].Begin(); itr != (*_doc)["bodies"].End(); ++itr)
            {
                _FillBody(*itr, _doc->GetAllocator());

                bool isRobot = false;
                LoadJsonValueByKey(*itr, "isRobot", isRobot);

                if (isRobot) {
                    _ExtractRobot(*itr, pprobot);
                    break;
                }
            }
            return true;
        }


        bool ExtractKinBody(KinBodyPtr& ppbody, const string& uri, const AttributesList& atts){
            // TODO
            std::string scheme, path, fragment;
            _ParseURI(uri, scheme, path, fragment);

            dReal fUnitScale = _GetUnitScale();
            
            if (fragment == "") {
                // if fragment is empty, take the first body in the scene
                rapidjson::Value::ValueIterator itr = (*_doc)["bodies"].Begin();
                std::string objectUri;
                // LoadJsonValueByKey(*itr, "uri", objectUri);
                _FillBody(*itr, _doc->GetAllocator());
                KinBodyPtr tmpBody = RaveCreateKinBody(_penv, "");
                // tmpBody->DeserializeJSON(*itr, fUnitScale);
                ppbody = tmpBody;
            } else {
                // find object by uri
                rapidjson::Value::ValueIterator object = _ResolveObjectInDocument(_doc, fragment);
                SetJsonValueByKey(*object, "uri", _CanonicalizeURI(uri), _doc->GetAllocator());
                KinBodyPtr tmpBody = RaveCreateKinBody(_penv, "");
                // tmpBody->DeserializeJSON(*object, fUnitScale);
                ppbody = tmpBody;
            }
            return true;
        }


        bool ExtractRobot(RobotBasePtr& pprobot, const string& uri, const AttributesList& atts){
            std::string scheme, path, fragment;
            _ParseURI(uri, scheme, path, fragment);

            dReal fUnitScale = _GetUnitScale(); // TODO(simon): use this

            if (fragment == "") {
                for (rapidjson::Value::ValueIterator itr = (*_doc)["bodies"].Begin(); itr != (*_doc)["bodies"].End(); ++itr)
                {
                    _FillBody(*itr, _doc->GetAllocator());
                    bool isRobot = false;
                    LoadJsonValueByKey(*itr, "isRobot", isRobot);
                    if (isRobot) {
                        _ExtractRobot(*itr, pprobot);
                        break;
                    }
                }
            } else {
                // find robot by uri
                rapidjson::Value::ValueIterator robot = _ResolveObjectInDocument(_doc, fragment);
                SetJsonValueByKey(*robot, "uri", _CanonicalizeURI(uri), _doc->GetAllocator());
                _ExtractRobot(*robot, pprobot);
            }
            return true;
        }

    protected:
        inline dReal _GetUnitScale()
        {
            std::pair<std::string, dReal> unit;
            LoadJsonValueByKey(*_doc, "unit", unit);
            if (unit.first == "mm")
            {
                unit.second *= 0.001;
            }
            else if (unit.first == "cm")
            {
                unit.second *= 0.01;
            }
            dReal scale = unit.second / _fGlobalScale;
            return scale;
        }

        std::string _CanonicalizeURI(const std::string& uri)
        {
            std::string scheme, path, fragment;
            _ParseURI(uri, scheme, path, fragment);

            if (scheme == "" && path == "") {
                if (_uri != "") {
                    std::string scheme2, path2, fragment2;
                    _ParseURI(_uri, scheme2, path2, fragment2);
                    return scheme2 + ":" + path2 + "#" + fragment;
                }
                return std::string("file:") + _filename + "#" + fragment;
            }
            return uri;
        }

        void _FillBody(rapidjson::Value &body, rapidjson::Document::AllocatorType& allocator)
        {
            std::string uri;
            LoadJsonValueByKey(body, "uri", uri);

            rapidjson::Value::ValueIterator object = _ResolveObject(uri);

            // add the object items into instobjects
            for (rapidjson::Value::MemberIterator it = object->MemberBegin(); it != object->MemberEnd(); ++it) {
                std::string key = it->name.GetString();
                if (key != "" && !body.HasMember(key.c_str())) {
                    rapidjson::Value key(key, allocator);
                    rapidjson::Value value(it->value, allocator);
                    body.AddMember(key, value, allocator);
                }
            }

            // fix the uri
            body.RemoveMember("uri");
            SetJsonValueByKey(body, "uri", _CanonicalizeURI(uri), allocator);
        }

        void _ExtractKinBody(const rapidjson::Value &value)
        {
            KinBodyPtr body = RaveCreateKinBody(_penv, "");
            dReal fUnitScale = _GetUnitScale();
            // body->DeserializeJSON(value, fUnitScale);
            _penv->Add(body, false);
        }

        void _ExtractKinBody(const rapidjson::Value &value, KinBodyPtr& ppbody)
        {
            KinBodyPtr body = RaveCreateKinBody(_penv, "");
            dReal fUnitScale = _GetUnitScale();
            // body->DeserializeJSON(value, fUnitScale);
            _penv->Add(body, false);
            ppbody = body;
        }

        void _ExtractRobot(const rapidjson::Value &value)
        {
            RobotBasePtr robot = RaveCreateRobot(_penv, "");
            dReal fUnitScale = _GetUnitScale();
            // robot->DeserializeJSON(value, fUnitScale);
            _penv->Add(robot, false);
        }

        void _ExtractRobot(const rapidjson::Value &value, RobotBasePtr& probot)
        {
            RobotBasePtr robot = RaveCreateRobot(_penv, "");
            dReal fUnitScale = _GetUnitScale();
            // robot->DeserializeJSON(value, fUnitScale);
            _penv->Add(robot, false);
            probot = robot;
        }

        /// \brief get the scheme of the uri, e.g. file: or openrave:
        void _ParseURI(const std::string& uri, std::string& scheme, std::string& path, std::string& fragment)
        {
            path = uri;
            size_t hashindex = path.find_last_of('#');
            if (hashindex != std::string::npos) {
                fragment = path.substr(hashindex + 1);
                path = path.substr(0, hashindex);
            }

            size_t colonindex = path.find_first_of(':');
            if (colonindex != std::string::npos) {
                // notice: in python code, like realtimerobottask3.py, it pass scheme as {openravescene: mujin}. No colon,
                scheme = path.substr(0, colonindex);
                path = path.substr(colonindex + 1);
            }

        }

        rapidjson::Value::ValueIterator _ResolveObject(const std::string &uri)
        {
            std::string scheme, path, fragment;
            _ParseURI(uri, scheme, path, fragment);

            std::string filename = _ResolveURI(scheme, path);
            boost::shared_ptr<rapidjson::Document> doc = _OpenDocument(filename);
            if (!doc) {
                throw OPENRAVE_EXCEPTION_FORMAT("failed resolve json document \"%s\"", uri, ORE_InvalidArguments);
            }
            return _ResolveObjectInDocument(doc, fragment);
        }

        void _IndexObjectsInDocument(boost::shared_ptr<rapidjson::Document> doc)
        {
            if (doc->IsObject() && doc->HasMember("objects") && (*doc)["objects"].IsArray())
            {
                for (rapidjson::Value::ValueIterator itr = (*doc)["objects"].Begin(); itr != (*doc)["objects"].End(); ++itr)
                {
                    rapidjson::Value::MemberIterator id = itr->FindMember("id");
                    if (id != itr->MemberEnd() && id->value.IsString()) {
                        std::string idstr = id->value.GetString();
                        if (idstr != "")
                        {
                            _objects[doc][idstr] = itr;
                        }
                    }
                }
            }
        }

        rapidjson::Value::ValueIterator _ResolveObjectInDocument(boost::shared_ptr<rapidjson::Document> doc, const std::string& id)
        {
            if (!!doc)
            {
                if (_objects.find(doc) == _objects.end())
                {
                    _IndexObjectsInDocument(doc);
                }
                if (_objects[doc].find(id) != _objects[doc].end())
                {
                    return _objects[doc][id];
                }
            }
            throw OPENRAVE_EXCEPTION_FORMAT("failed resolve object \"%s\" in json document", id, ORE_InvalidArguments);
        }

        /// \brief resolve a uri
        std::string _ResolveURI(const std::string& uri)
        {
            std::string scheme, path, fragment;
            _ParseURI(uri, scheme, path, fragment);

            return _ResolveURI(scheme, path);
        }

        std::string _ResolveURI(const std::string& scheme, const std::string& path)
        {
            if (scheme == "" && path == "")
            {
                return _filename;
            }
            else if (scheme == "file")
            {
                return RaveFindLocalFile(path);
            }
            else if(find(_vOpenRAVESchemeAliases.begin(), _vOpenRAVESchemeAliases.end(), scheme) != _vOpenRAVESchemeAliases.end())
            {
                return RaveFindLocalFile(path);
                // TODO deal with openrave: or mujin:
            }
            
            return "";
        }

        /// \brief open and cache a json document
        boost::shared_ptr<rapidjson::Document> _OpenDocument(const std::string& filename)
        {
            if (_docs.find(filename) != _docs.end()) {
                return _docs[filename];
            }
            std::ifstream ifs(filename.c_str());
            rapidjson::IStreamWrapper isw(ifs);
            boost::shared_ptr<rapidjson::Document> doc;
            doc.reset(new rapidjson::Document);
            rapidjson::ParseResult ok = doc->ParseStream(isw);
            if (!ok) {
                throw OPENRAVE_EXCEPTION_FORMAT("failed parse json document \"%s\"", filename, ORE_InvalidArguments);
            }

            _docs[filename] = doc;
            return doc;
        }

        /// \brief parse and cache a json document
        boost::shared_ptr<rapidjson::Document> _ParseDocument(const std::string& data)
        {
            boost::shared_ptr<rapidjson::Document> doc;
            doc.reset(new rapidjson::Document);
            rapidjson::ParseResult ok = doc->Parse(data.c_str());
            if (!ok) {
                throw OPENRAVE_EXCEPTION_FORMAT0("failed parse json document", ORE_InvalidArguments);
            }

            _docs[""] = doc;
            return doc;
        }

        dReal _fGlobalScale;
        EnvironmentBasePtr _penv;
        std::string _prefix;
        std::string _filename;
        std::string _uri;
        std::vector<std::string> _vOpenRAVESchemeAliases;
        boost::shared_ptr<rapidjson::Document> _doc;
        std::map<std::string, boost::shared_ptr<rapidjson::Document> > _docs; ///< key is filename
        std::map<boost::shared_ptr<rapidjson::Document>, std::map<std::string, rapidjson::Value::ValueIterator> > _objects; ///< key is pointer to doc
    };

    bool RaveParseJSONFile(EnvironmentBasePtr penv, const std::string& filename, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        std::string fullFilename = RaveFindLocalFile(filename);
        if (fullFilename.size() == 0 || !reader.InitFromFile(fullFilename)) {
            return false;
        }
        return reader.Extract();
    }

    bool RaveParseJSONFile(EnvironmentBasePtr penv, KinBodyPtr& ppbody, const std::string& filename, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        std::string fullFilename = RaveFindLocalFile(filename);
        if (fullFilename.size() == 0 || !reader.InitFromFile(fullFilename)) {
            return false;
        }
        return reader.Extract(ppbody);
    }

    bool RaveParseJSONFile(EnvironmentBasePtr penv, RobotBasePtr& pprobot, const std::string& filename, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        std::string fullFilename = RaveFindLocalFile(filename);
        if (fullFilename.size() == 0 || !reader.InitFromFile(fullFilename)) {
            return false;
        }
        return reader.Extract(pprobot);
    }

    bool RaveParseJSONURI(EnvironmentBasePtr penv, const std::string& uri, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        if (!reader.InitFromURI(uri)) {
            return false;
        }
        return reader.Extract();
    }

    bool RaveParseJSONURI(EnvironmentBasePtr penv, KinBodyPtr& ppbody, const std::string& uri, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        if (!reader.InitFromURI(uri)) {
            return false;
        }
        return reader.ExtractKinBody(ppbody, uri, atts);
    }

    bool RaveParseJSONURI(EnvironmentBasePtr penv, RobotBasePtr& pprobot, const std::string& uri, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        if (!reader.InitFromURI(uri)) {
            return false;
        }
        // TODO
        return reader.ExtractRobot(pprobot, uri, atts);
    }

    bool RaveParseJSONData(EnvironmentBasePtr penv, const std::string& data, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        if (!reader.InitFromData(data)) {
            return false;
        }
        return reader.Extract();
    }

    bool RaveParseJSONData(EnvironmentBasePtr penv, KinBodyPtr& ppbody, const std::string& data, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        if (!reader.InitFromData(data)) {
            return false;
        }
        // TODO
        // return reader.ExtractKinBody(ppbody, )
        return false;
    }

    bool RaveParseJSONData(EnvironmentBasePtr penv, RobotBasePtr& pprobot, const std::string& data, const AttributesList& atts)
    {
        JSONReader reader(atts, penv);
        if (!reader.InitFromData(data)) {
            return false;
        }
        // TODO
        return false;
    }
}
