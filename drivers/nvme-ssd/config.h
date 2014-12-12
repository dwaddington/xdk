/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  
*/






#ifndef __NVME_CONFIG_H__
#define __NVME_CONFIG_H__

#include <tinyxml.h>


// class Visitor : public TiXmlVisitor
// {
// public:
// 	virtual ~Visitor() {
//   }

// 	/// Visit a document.
// 	bool VisitEnter( const TiXmlDocument& doc )			{ 
//     return true; 
//   }

// 	/// Visit a document.
// 	bool VisitExit( const TiXmlDocument& doc )			{ 
//     return true; 
//   }

// 	/// Visit an element.
// 	bool VisitEnter( const TiXmlElement& element, const TiXmlAttribute* firstAttribute )	{ 
//     printf("Visiting element %s\n",element
//     return true; 
//   }

// 	/// Visit an element.
// 	bool VisitExit( const TiXmlElement& element )		{ 
//     return true; 
//   }

// 	/// Visit a declaration
// 	bool Visit( const TiXmlDeclaration& declaration )	{ 
//     return true; 
//   }

// 	/// Visit a text node
// 	bool Visit( const TiXmlText& text )	{ 
//     return true; 
//   }

// 	/// Visit a comment node
// 	bool Visit( const TiXmlComment& comment )	{ 
//     return true; 
//   }

// 	/// Visit an unknown node
// 	bool Visit( const TiXmlUnknown& /*unknown*/ )	{ 
//     return true; 
//   }

// };

class Config_IO_Queues
{
private:
  struct mapping {
    unsigned queue_id;
    core_id_t core;
    mapping(unsigned q, core_id_t c) : queue_id(q), core(c) {}
  };

  unsigned _num_queues;
  std::vector< struct mapping > _core_qid_mappings;
  unsigned _cores_len;

public:
  Config_IO_Queues() : _num_queues(0), _cores_len(64) {
  }
  
  void add_IO_queue(const char * id, const char * core) {
    _num_queues++;
    int i = atoi(id);
    int c = atoi(core);    
    assert(c <= sysconf(_SC_NPROCESSORS_ONLN));
    
    struct mapping m(i,c);
    _core_qid_mappings.push_back(m);
  }

  unsigned num_io_queues() const { return _num_queues; }
  unsigned io_queue_get_assigned_core(unsigned index, core_id_t * core) { 
    assert(index < _core_qid_mappings.size());
    *core = _core_qid_mappings[index].core;
    return _core_qid_mappings[index].queue_id;
  }
  core_id_t get_core(unsigned index) {
    assert(index < _core_qid_mappings.size());
    return _core_qid_mappings[index].core;
  }
};

class Config_queue_config
{
private:
  unsigned _io_queue_len;
public:
  void set_io_queue_len(const char * lenstr) {
    _io_queue_len = atoi(lenstr);
  }
  unsigned get_io_queue_len() const { return _io_queue_len; }
};

class Config : public Config_IO_Queues,
               public Config_queue_config
{
public:
  class Config_exception : public Exokernel::Exception {
  public:
    Config_exception(const char * cause) : Exokernel::Exception(cause) {
      PERR("xml config error (%s)",cause);
    }
  };


private:
  TiXmlDocument _doc;
  TiXmlHandle   _root;

  void parse_config() {
    _root = TiXmlHandle(_doc.RootElement()).Element();
    
    /* IO_queue_config */
    {
      try {
        TiXmlElement * e = _root.FirstChild("IO_queue_config").Element();
        set_io_queue_len(e->Attribute("length"));
      }
      catch(...) {
        throw Config_exception("cannot find IO_queue_config node");
      }
    }
    /* IO_queues */
    {
      TiXmlElement * e = _root.FirstChild("IO_queues").Element();
      if(!e) 
        throw Config_exception("cannot find IO_Queues node");

      TiXmlElement * child = e->FirstChildElement();
      while(child) {
        /* Completion_queues */
        if(child->ValueStr()=="Completion_queue") {
          add_IO_queue(child->Attribute("id"), child->Attribute("core"));
        }
        child = child->NextSiblingElement();
      }
    }    
    
  }


public:
  Config(const char * filename) :_root(0) {
    NVME_INFO("config file:%s\n", filename);
    if(!_doc.LoadFile(filename))
      throw Exokernel::Exception("unable to open configuration file");

    parse_config();
  }

  ~Config() {
  }
};


#endif // __NVME_CONFIG_H__
