#ifndef HASH_MAP_HPP_
#define HASH_MAP_HPP_

#include <string>
#include <iostream>
#include <sstream>
#include <initializer_list>
#include "ics_exceptions.hpp"
#include "pair.hpp"


namespace ics {


#ifndef undefinedhashdefined
#define undefinedhashdefined
    template<class T>
    int undefinedhash (const T& a) {return 0;}
#endif /* undefinedhashdefined */

//Instantiate the templated class supplying thash(a): produces a hash value for a.
//If thash is defaulted to undefinedhash in the template, then a constructor must supply chash.
//If both thash and chash are supplied, then they must be the same (by ==) function.
//If neither is supplied, or both are supplied but different, TemplateFunctionError is raised.
//The (unique) non-undefinedhash value supplied by thash/chash is stored in the instance variable hash.
    template<class KEY,class T, int (*thash)(const KEY& a) = undefinedhash<KEY>> class HashMap {
    public:
        typedef ics::pair<KEY,T>   Entry;
        typedef int (*hashfunc) (const KEY& a);

        //Destructor/Constructors
        ~HashMap ();

        HashMap          (double the_load_threshold = 1.0, int (*chash)(const KEY& a) = undefinedhash<KEY>);
        explicit HashMap (int initial_bins, double the_load_threshold = 1.0, int (*chash)(const KEY& k) = undefinedhash<KEY>);
        HashMap          (const HashMap<KEY,T,thash>& to_copy, double the_load_threshold = 1.0, int (*chash)(const KEY& a) = undefinedhash<KEY>);
        explicit HashMap (const std::initializer_list<Entry>& il, double the_load_threshold = 1.0, int (*chash)(const KEY& a) = undefinedhash<KEY>);

        //Iterable class must support "for-each" loop: .begin()/.end() and prefix ++ on returned result
        template <class Iterable>
        explicit HashMap (const Iterable& i, double the_load_threshold = 1.0, int (*chash)(const KEY& a) = undefinedhash<KEY>);


        //Queries
        bool empty      () const;
        int  size       () const;
        bool has_key    (const KEY& key) const;
        bool has_value  (const T& value) const;
        std::string str () const; //supplies useful debugging information; contrast to operator <<


        //Commands
        T    put   (const KEY& key, const T& value);
        T    erase (const KEY& key);
        void clear ();

        //Iterable class must support "for-each" loop: .begin()/.end() and prefix ++ on returned result
        template <class Iterable>
        int put_all(const Iterable& i);


        //Operators

        T&       operator [] (const KEY&);
        const T& operator [] (const KEY&) const;
        HashMap<KEY,T,thash>& operator = (const HashMap<KEY,T,thash>& rhs);
        bool operator == (const HashMap<KEY,T,thash>& rhs) const;
        bool operator != (const HashMap<KEY,T,thash>& rhs) const;

        template<class KEY2,class T2, int (*hash2)(const KEY2& a)>
        friend std::ostream& operator << (std::ostream& outs, const HashMap<KEY2,T2,hash2>& m);



    private:
        class LN;

    public:
        class Iterator {
        public:
            typedef pair<int,LN*> Cursor;

            //Private constructor called in begin/end, which are friends of HashMap<T>
            ~Iterator();
            Entry       erase();
            std::string str  () const;
            HashMap<KEY,T,thash>::Iterator& operator ++ ();
            HashMap<KEY,T,thash>::Iterator  operator ++ (int);
            bool operator == (const HashMap<KEY,T,thash>::Iterator& rhs) const;
            bool operator != (const HashMap<KEY,T,thash>::Iterator& rhs) const;
            Entry& operator *  () const;
            Entry* operator -> () const;
            friend std::ostream& operator << (std::ostream& outs, const HashMap<KEY,T,thash>::Iterator& i) {
                outs << i.str(); //Use the same meaning as the debugging .str() method
                return outs;
            }
            friend Iterator HashMap<KEY,T,thash>::begin () const;
            friend Iterator HashMap<KEY,T,thash>::end   () const;

        private:
            //If can_erase is false, current indexes the "next" value (must ++ to reach it)
            Cursor                current; //Pair:Bin Index/LN*; stops if LN* == nullptr
            HashMap<KEY,T,thash>* ref_map;
            int                   expected_mod_count;
            bool                  can_erase = true;

            //Helper methods
            void advance_cursors();

            //Called in friends begin/end
            Iterator(HashMap<KEY,T,thash>* iterate_over, bool from_begin);
        };


        Iterator begin () const;
        Iterator end   () const;


    private:
        class LN {
        public:
            LN ()                         : next(nullptr){}
            LN (const LN& ln)             : value(ln.value), next(ln.next){}
            LN (Entry v, LN* n = nullptr) : value(v), next(n){}

            Entry value;
            LN*   next;
        };

        int (*hash)(const KEY& k);  //Hashing function used (from template or constructor)
        LN** map      = nullptr;    //Pointer to array of pointers: each bin stores a list with a trailer node
        double load_threshold;      //used/bins <= load_threshold
        int bins      = 1;         //# bins currently in array (start it >= 1 so no divide by 0 in hash_compress)
        int used      = 0;          //Cache for number of key->value pairs in the hash table
        int mod_count = 0;          //For sensing concurrent modification


        //Helper methods
        int   hash_compress        (const KEY& key)          const;  //hash function ranged to [0,bins-1]
        LN*   find_key             (const KEY& key) const;           //Returns reference to key's node or nullptr
        LN*   copy_list            (LN*   l)                 const;  //Copy the keys/values in a bin (order irrelevant)
        LN**  copy_hash_table      (LN** ht, int bins)       const;  //Copy the bins/keys/values in ht tree (order in bins irrelevant)

        void  ensure_load_threshold(int new_used);                   //Reallocate if load_factor > load_threshold
        void  delete_hash_table    (LN**& ht, int bins);             //Deallocate all LN in ht (and the ht itself; ht == nullptr)
    };





////////////////////////////////////////////////////////////////////////////////
//
//HashMap class and related definitions

//Destructor/Constructors

    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::~HashMap() {
        delete_hash_table(map, bins);
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::HashMap(double the_load_threshold, int (*chash)(const KEY& k))
            :load_threshold(the_load_threshold){//,hash(thash != (hashfunc)undefinedhash<T> ? thash : chash){
        if(thash != (hashfunc)undefinedhash<KEY>)
            hash = thash;
        else
            hash = chash;
        if (hash == (hashfunc)undefinedhash<KEY>)
            throw TemplateFunctionError("HashMap::default constructor: neither specified");
        if (thash != (hashfunc)undefinedhash<KEY> && chash != (hashfunc)undefinedhash<KEY> && thash != chash)
            throw TemplateFunctionError("HashMap::default constructor: both specified and different");
        map = new LN*[bins];
        map[0] = new LN();  // Trailer node
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::HashMap(int initial_bins, double the_load_threshold, int (*chash)(const KEY& k))
            : hash(thash != (hashfunc)undefinedhash<KEY> ? thash : chash), bins(initial_bins), load_threshold(the_load_threshold){
        if (hash == (hashfunc)undefinedhash<KEY>)
            throw TemplateFunctionError("HashMap::bins constructor: neither specified");
        if (thash != (hashfunc)undefinedhash<KEY> && chash != (hashfunc)undefinedhash<KEY> && thash != chash)
            throw TemplateFunctionError("HashMap::bins constructor: both specified and different");
        map = new LN*[bins];
        for(int i = 0; i < bins; ++i){
            map[i] = new LN();
        }
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::HashMap(const HashMap<KEY,T,thash>& to_copy, double the_load_threshold, int (*chash)(const KEY& a))
            : hash(thash != (hashfunc)undefinedhash<KEY> ? thash : chash), load_threshold(the_load_threshold){
        if (hash == (hashfunc)undefinedhash<KEY>)
            hash = chash;//throw TemplateFunctionError("HashMap::copy constructor: neither specified");
        if (thash != (hashfunc)undefinedhash<KEY> && chash != (hashfunc)undefinedhash<KEY> && thash != chash)
            throw TemplateFunctionError("HashMap::copy constructor: both specified and different");
        bins = to_copy.bins;
        map = new LN*[bins];
        for(int i = 0; i < bins; ++i){
            map[i] = new LN();
        }
        if(hash != to_copy.hash)
            used = put_all(to_copy);
        else{
            map = copy_hash_table(to_copy.map, to_copy.bins);
            used = to_copy.used;
        }
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::HashMap(const std::initializer_list<Entry>& il, double the_load_threshold, int (*chash)(const KEY& k))
            : hash(thash != (hashfunc)undefinedhash<KEY> ? thash : chash), load_threshold(the_load_threshold){
        if (hash == (hashfunc)undefinedhash<KEY>)
            throw TemplateFunctionError("HashMap::initializer_list constructor: neither specified");
        if (thash != (hashfunc)undefinedhash<KEY> && chash != (hashfunc)undefinedhash<KEY> && thash != chash)
            throw TemplateFunctionError("HashMap::initializer_list constructor: both specified and different");
        map = new LN*[bins];
        map[0] = new LN();
        for (const Entry& m_entry : il)
            put(m_entry.first,m_entry.second);
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    template <class Iterable>
    HashMap<KEY,T,thash>::HashMap(const Iterable& i, double the_load_threshold, int (*chash)(const KEY& k))
            : hash(thash != (hashfunc)undefinedhash<T> ? thash : chash), load_threshold(the_load_threshold){
        if (hash == (hashfunc)undefinedhash<KEY>)
            throw TemplateFunctionError("BSTMap::Iterable constructor: neither specified");
        if (thash != (hashfunc)undefinedhash<KEY> && chash != (hashfunc)undefinedhash<KEY> && thash != chash)
            throw TemplateFunctionError("BSTMap::Iterable constructor: both specified and different");
        map = new LN*[bins];
        map[0] = new LN();
        for (const Entry& m_entry : i){
            put(m_entry.first,m_entry.second);
        }
    }


////////////////////////////////////////////////////////////////////////////////
//
//Queries

    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::empty() const {
        return used == 0;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    int HashMap<KEY,T,thash>::size() const {
        return used;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::has_key (const KEY& key) const {
        for(int i = 0; i < bins; ++i){
            for(LN*j = map[i]; j->next != nullptr; j = j->next){
//                std::cout << "j->value.first : " << j->value.first << std::endl;
//                std::cout << "key: " << key << std::endl;
//                std::cout << j->value << std::endl;
                if(j->value.first == key)
                    return true;
            }
        }
        return false;
        //return find_key(key) != nullptr;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::has_value (const T& value) const {
        for(int i = 0; i < bins; ++i){
            for(LN*j = map[i]; j->next != nullptr; j = j->next){
                if(j->value.second == value)
                    return true;
            }
        }
        return false;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    std::string HashMap<KEY,T,thash>::str() const {
        std::ostringstream answer;
        for(int i = 0; i < bins; ++i){
            for(LN* j = map[i]; j->next != nullptr; j = j->next){
                answer << j->value.first << "->" << j->value.second;
            }
        }
        return answer.str();
    }


////////////////////////////////////////////////////////////////////////////////
//
//Commands

    template<class KEY,class T, int (*thash)(const KEY& a)>
    T HashMap<KEY,T,thash>::put(const KEY& key, const T& value) {
        int hash_index = hash_compress(key);
        if(has_key(key)){
            LN* found_key = find_key(key);
//            std::cout << "key is: " << key << std::endl;
//            for(LN* i = map[hash_index]; i != nullptr; i = i->next){
//                std::cout << "i->value.first is: " << i->value.first << std::endl;
//                if(i->value.first == key){
//                    T old_value = i->value.second;
//                    i->value.second = value;
//                    return old_value;
//                }
//            }
            T old_value = found_key->value.second;
            found_key->value.second = value;
            return old_value;
        }
        else{
            ensure_load_threshold(++used);
            std::cout<< "Hash_index: " << hash_index << std::endl;
            map[hash_index] = new LN(Entry(key,value), map[hash_index]);
        }
        ++mod_count;
        return map[hash_index]->value.second;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    T HashMap<KEY,T,thash>::erase(const KEY& key) {
        LN* current = find_key(key);
        if(current != nullptr){
            T to_return = current->value.second;
            LN* to_delete = current->next;
            *current = *current->next;
            delete to_delete;
            --used;
            ++mod_count;
            return to_return;
        }
        std::ostringstream answer;
        answer << "HashMap::erase: key(" << key << ") not in Hash";
        throw KeyError(answer.str());
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    void HashMap<KEY,T,thash>::clear() {
        for (int i = 0; i < bins; ++i){
            for (LN* j = map[i]; j != nullptr;){
                if(j->next == nullptr){
                    map[i] = j;
                    j = nullptr;
                    break;
                }
                LN* to_delete = j;
                j = j->next;
                delete to_delete;
            }
        }
        used = 0;
        ++mod_count;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    template<class Iterable>
    int HashMap<KEY,T,thash>::put_all(const Iterable& i) {
        int count = 0;
        for (const Entry& m_entry : i){
            ++count;
            put(m_entry.first,m_entry.second);
        }
        return count;
    }


////////////////////////////////////////////////////////////////////////////////
//
//Operators

    template<class KEY,class T, int (*thash)(const KEY& a)>
    T& HashMap<KEY,T,thash>::operator [] (const KEY& key) {
        LN* current = find_key(key);
        if(current != nullptr)
            return current->value.second;
        T empty;
        put(key, empty);
        return find_key(key)->value.second;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    const T& HashMap<KEY,T,thash>::operator [] (const KEY& key) const {
        LN* current = find_key(key);
        if(current != nullptr)
            return current->value.second;

        std::ostringstream answer;
        answer << "HashMap::operator []: key(" << key << ") not in Map";
        throw KeyError(answer.str());
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>& HashMap<KEY,T,thash>::operator = (const HashMap<KEY,T,thash>& rhs) {
        if(this == &rhs)
            return *this;
        load_threshold = rhs.load_threshold;
        bins = rhs.bins;
        used = rhs.used;
        hash = rhs.hash;
        map = new LN*[bins];
        for(int i = 0; i < bins; ++i){
            map[i] = new LN();
        }
        map = copy_hash_table(rhs.map, rhs.bins);

        return *this;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::operator == (const HashMap<KEY,T,thash>& rhs) const {
        if (this == &rhs)
            return true;

        if (used != rhs.used)
            return false;

        for(int i = 0; i < bins; ++i){
            for(LN* current = map[i]; current->next != nullptr; current = current->next){
                if(current->value.first == "")
                    break;
                if(current->value.second != rhs[current->value.first])
                    return false;
            }
        }
        return true;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::operator != (const HashMap<KEY,T,thash>& rhs) const {
        return !(*this == rhs);
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    std::ostream& operator << (std::ostream& outs, const HashMap<KEY,T,thash>& m) {
        outs << "map[";
        if(!m.empty())
            outs << m.str();
        outs << "]";
        return outs;
    }


////////////////////////////////////////////////////////////////////////////////
//
//Iterator constructors

    template<class KEY,class T, int (*thash)(const KEY& a)>
    auto HashMap<KEY,T,thash>::begin () const -> HashMap<KEY,T,thash>::Iterator {
        return Iterator(const_cast<HashMap<KEY,T,thash>*>(this),true);
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    auto HashMap<KEY,T,thash>::end () const -> HashMap<KEY,T,thash>::Iterator {
        return Iterator(const_cast<HashMap<KEY,T,thash>*>(this),false);
    }


///////////////////////////////////////////////////////////////////////////////
//
//Private helper methods

    template<class KEY,class T, int (*thash)(const KEY& a)>
    int HashMap<KEY,T,thash>::hash_compress (const KEY& key) const {    // Not sure I'm doing this right
        return abs(hash(key)) % bins;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    typename HashMap<KEY,T,thash>::LN* HashMap<KEY,T,thash>::find_key (const KEY& key) const {
        for(int i = 0; i < bins; ++i) {
            for(LN* j = map[i]; j != nullptr; j = j->next) {
                if(j->value.first == key)
                    return j;
            }
        }
        return nullptr;

    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    typename HashMap<KEY,T,thash>::LN* HashMap<KEY,T,thash>::copy_list (LN* l) const {
        if(l->next == nullptr)
            return new LN();
        return new LN(l->value, copy_list(l->next));
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    typename HashMap<KEY,T,thash>::LN** HashMap<KEY,T,thash>::copy_hash_table (LN** ht, int bins) const {   // Calls copy_list
        for(int i = 0; i < bins; ++i){
            map[i] = copy_list(ht[i]);
        }
        return map;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    void HashMap<KEY,T,thash>::ensure_load_threshold(int new_used) {
        if (new_used / bins <= load_threshold)
            return;
        LN** old_map = map;
        bins *= 2;
        map = new LN*[bins];
        for (int i=0; i<bins; ++i)
            map[i] = new LN();
        for (int i = 0; i < bins/2; ++i){
            for(LN* j = old_map[i]; j != nullptr ; j = j->next){
                map[hash_compress(j->value.first)] = new LN(Entry(j->value.first,j->value.second), map[hash_compress(j->value.first)]);
            }
        }
        delete old_map;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    void HashMap<KEY,T,thash>::delete_hash_table (LN**& ht, int bins) {
        for (int i = 0; i < bins; ++i){
            for (LN* j = ht[i]; j != nullptr;){
                LN* to_delete = j;
                j = j->next;
                delete to_delete;
            }
        }
        delete [] map;
    }


////////////////////////////////////////////////////////////////////////////////
//
//Iterator class definitions

    template<class KEY,class T, int (*thash)(const KEY& a)>
    void HashMap<KEY,T,thash>::Iterator::advance_cursors(){
        //if(current.second->next->next != nullptr)
        //std::cout << "current's pair is: " << current.second->value << std::endl;
        //if(current.second->next != nullptr && current.second->next->value.first != "")//works but something is wrong though
        //if(current.second->next->next != nullptr)
        if(current.second->next->value.first != "")
            current.second = current.second->next;
        else{
//            std::cout << ref_map->bins << std::endl;
            for(int i = current.first + 1; i < ref_map->bins; ++i){
                for(LN* j = ref_map->map[i]; j->next != nullptr; j = j->next){
                    current.first = i;
//                    std::cout << j->value << std::endl;
                    if(j->value.first != ""){
                        current.second = j;
                        return;
                    }
                }
            }
            current.first = -1;
            current.second = nullptr;
        }
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::Iterator::Iterator(HashMap<KEY,T,thash>* iterate_over, bool from_begin)
            : ref_map(iterate_over), expected_mod_count(ref_map->mod_count) {
        if(!from_begin || ref_map->empty()){
            current.first = -1;
            current.second = nullptr;
        }
        else{
            bool check = false;
            for(int i = 0; i < ref_map->bins; ++i){
                for(LN* j = ref_map->map[i]; j->next != nullptr;){
                    current.first = i;
                    current.second = j;
                    check = true;
                    break;
                }
                if(check)
                    break;
                //if(current.first == i)
            }
//            current.first = 0;
//            current.second = ref_map->map[0];
        }
//        for(int i = 0; i < ref_map->bins; ++i){
//            current.first = i;
//            for(LN* node = ref_map->map[i]; node != nullptr; node = node->next){
//                if(node->next != nullptr){
//                    current.second = node;
//                    break;
//                }
//            }
//        }

        //for(int i = 0; i < ref_map->bins; ++i){
        //for(LN* node = ref_map->map[i]; node != nullptr; node = node->next)

    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    HashMap<KEY,T,thash>::Iterator::~Iterator()
    {}


    template<class KEY,class T, int (*thash)(const KEY& a)>
    auto HashMap<KEY,T,thash>::Iterator::erase() -> Entry {
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::erase");
        if (!can_erase)
            throw CannotEraseError("BSTMap::Iterator::erase Iterator cursor already erased");
        if (current.second == nullptr)
            throw CannotEraseError("BSTMap::Iterator::erase Iterator cursor beyond data structure");

        can_erase = false;
        //if(current.second->value.first == "")
        Entry to_return = current.second->value;
        LN* to_delete = current.second;
        if(current.second->next->next == nullptr)
            advance_cursors();
        ref_map->erase(to_delete->value.first);
        expected_mod_count = ref_map->mod_count;

        return to_return;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    std::string HashMap<KEY,T,thash>::Iterator::str() const {
        std::ostringstream answer;
        answer << ref_map->str() << "(expected_mod_count=" << expected_mod_count  << ",can_erase=" << can_erase << ")";
        return answer.str();
    }

    template<class KEY,class T, int (*thash)(const KEY& a)>
    auto  HashMap<KEY,T,thash>::Iterator::operator ++ () -> HashMap<KEY,T,thash>::Iterator& {
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::operator ++");

        if (current.second == nullptr)
            return *this;

        if (can_erase)
            advance_cursors();
        else
            can_erase = true;

        return *this;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    auto  HashMap<KEY,T,thash>::Iterator::operator ++ (int) -> HashMap<KEY,T,thash>::Iterator {
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::operator ++");

        if (current.second == nullptr)
            return *this;

        Iterator to_return(*this);
        if (can_erase)
            advance_cursors();
        else
            can_erase = true;

        return to_return;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::Iterator::operator == (const HashMap<KEY,T,thash>::Iterator& rhs) const {
        const Iterator* rhsASI = dynamic_cast<const Iterator*>(&rhs);
        if (rhsASI == 0)
            throw IteratorTypeError("BSTMap::Iterator::operator ==");
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::operator ==");
        if (ref_map != rhsASI->ref_map)
            throw ComparingDifferentIteratorsError("BSTMap::Iterator::operator ==");

        return current.second == rhs.current.second;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    bool HashMap<KEY,T,thash>::Iterator::operator != (const HashMap<KEY,T,thash>::Iterator& rhs) const {
        const Iterator* rhsASI = dynamic_cast<const Iterator*>(&rhs);
        if (rhsASI == 0)
            throw IteratorTypeError("BSTMap::Iterator::operator !=");
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::operator !=");
        if (ref_map != rhsASI->ref_map)
            throw ComparingDifferentIteratorsError("BSTMap::Iterator::operator !=");

        return current.second != rhs.current.second;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    pair<KEY,T>& HashMap<KEY,T,thash>::Iterator::operator *() const {
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::operator *");
        if (!can_erase || current.second == nullptr) {
            std::ostringstream where;
            where << current << " when size = " << ref_map->size();
            throw IteratorPositionIllegal("BSTMap::Iterator::operator * Iterator illegal: " + where.str());
        }
        return current.second->value;
    }


    template<class KEY,class T, int (*thash)(const KEY& a)>
    pair<KEY,T>* HashMap<KEY,T,thash>::Iterator::operator ->() const {
        if (expected_mod_count != ref_map->mod_count)
            throw ConcurrentModificationError("BSTMap::Iterator::operator ->");
        if (!can_erase || current->second == nullptr) {
            std::ostringstream where;
            where << current << " when size = " << ref_map->size();
            throw IteratorPositionIllegal("BSTMap::Iterator::operator -> Iterator illegal: " + where.str());
        }
        return &current.second->value;
    }


}

#endif /* HASH_MAP_HPP_ */
