//
//  package.h
//  ePub3
//
//  Created by Jim Dovey on 2012-11-28.
//  Copyright (c) 2012-2013 The Readium Foundation.
//  
//  The Readium SDK is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//  
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __ePub3__package__
#define __ePub3__package__

#include "epub3.h"
#include <string>
#include <vector>
#include <map>
#include <list>
#include <libxml/tree.h>
#include "spine.h"
#include "manifest.h"
#include "metadata.h"
#include "cfi.h"
#include "nav_element.h"
#include "archive_xml.h"
#include "utfstring.h"
#include "iri.h"

EPUB3_BEGIN_NAMESPACE

class Archive;
class Metadata;
class NavigationTable;

/**
 The PackageBase class implements the low-level components and all storage of an OPF
 package document.  It provides direct access to spine, manifest, and metadata tables,
 while the Package class provides a higher-level API on top of these.
 */
class PackageBase
{
public:
    ///
    /// An array of Metadata items, in document order.
    typedef std::vector<Metadata*>              MetadataMap;
    ///
    /// A lookup table for navigation tables, indexed by type.
    typedef std::map<string, NavigationTable*>  NavigationMap;
    ///
    /// A lookup table for property vocabulary IRI stubs, indexed by prefix.
    typedef std::map<string, string>            PropertyVocabularyMap;
    ///
    /// An array of concrete property IRIs.
    typedef std::vector<IRI>                    PropertyList;
    
    /**
     This exception is thrown when a property vocabulary prefix is unknown to a
     given Package.
     
     A prefix is considered *unknown* when it is neither in the ePub3 predefined set
     nor was it explicitly declared in the package document's `<package>` tag
     through a `prefix` attribute.
     */
    class UnknownPrefix : public std::domain_error
    {
    public:
                    UnknownPrefix(const string &str)    : std::domain_error(str.stl_str()) {}
                    UnknownPrefix(const char* str)      : std::domain_error(str) {}
        virtual     ~UnknownPrefix() {}
    };
    
public:
    /** There is no default constructor for PackageBase. */
                            PackageBase() = delete;
    /**
     Constructs a new PackageBase class by reading an XML document from a given
     Archive using the supplied path.
     
     The type, at present, is assumed to be `application/oebps-package+xml`; the
     parameter is here for future-proofing in case of alternative package types in
     later standards.
     @param archive The Archive from which to read the package document.
     @param path The path of the document within the archive.
     @param type The MIME type of the document, as read from the OCF `root-file`
     element.
     */
                            PackageBase(Archive * archive, const string& path, const string& type);
    /** There is no copy constructor for PackageBase. */
                            PackageBase(const PackageBase&) = delete;
    /** C++11 'move' constructor-- claims ownership of its argument's internals. */
                            PackageBase(PackageBase&&);
    virtual                 ~PackageBase();
    
    /**
     Returns the path used to construct this object minus the filename, e.g.
     
     ~~~{.cpp}
     Package pkg(&myArchive, "EPUB/package.opf", theType);
     assert(pkg.BasePath() == "EPUB/");
     ~~~
     */
    virtual const string&   BasePath()              const       { return _pathBase; }
    
    /**
     @defgroup TableAccess Raw Table Accessors
     @note To keep these accessible in const packages, we *must* build the tree at
     allocation time.  This is open to discussion, naturally.
     @{
     */
    
    ///
    /// Returns an immutable reference to the metadata table.
    const MetadataMap&      Metadata()              const       { return _metadata; }
    ///
    /// Returns an immutable reference to the manifest table.
    const ManifestTable&    Manifest()              const       { return _manifest; }
    ///
    /// Returns an immutable reference to the map of navigation tables.
    const NavigationMap&    NavigationTables()      const       { return _navigation; }
    
    /** @} */
    
    /**
     Obtains an IRI for a DCMES metadata item.
     @note The IRIs we use for DCMES items are not canon for ePub3.  We use them
     only for the benefit of using a single method to identify metadata items
     whether defined using DCMES elements or regular `<meta>` elements with an
     IRI-based property.
     @param type A type-code for a DCMES metadata item.
     @result A constant IRI referring to the type, or an empty (invalid) IRI if the
     `type` constant does not refer to a valid DCMES element (i.e. the `Custom` or
     `Invalid` pseudo-types).
     */
    const IRI               IRIForDCType(Metadata::DCType type) const { return Metadata::IRIForDCType(type); }
    
    /**
     @defgroup SpineAccess Spine Accessors
     @{
     */
    
    /**
     Returns the first item in the Spine.
     */
    const SpineItem *       FirstSpineItem()        const { return _spine.get(); }
    
    /**
     Locates a spine item by position.
     @param idx The zero-based position of the item to return.
     @result A pointer to the requested spine item, or `nullptr` if the index was
     out of bounds.
     */
    const SpineItem *       SpineItemAt(size_t idx) const;
    
    size_t                  IndexOfSpineItemWithIDRef(const string& idref)  const;
    
    /** @} */
    
    /**
     @defgroup ManifestAccess Manifest Item Accessors
     @{
     */
    
    /**
     Looks up and returns a specific manifest item by its unique identifier.
     @param ident The unique identifier for the item to retrieve.
     @result A pointer to the requested item, or `nullptr` if no such item exists.
     */
    const ManifestItem *    ManifestItemWithID(const string& ident)         const;
    
    /**
     Generates the subpath part of a CFI used to locate a given manifest item.
     
     The subpath will contain the correct steps to locate the requested item through
     the spine.  This fragment will include an element qualifier, e.g.
     
     ~~~{.cpp}
     string s = pkg.CFISubpathForManifestItemWithID("item1");
     // returns "/6/4[item1]!"
     ~~~
     @param ident The unique identifier for the manifest item to locate.
     @result A string containing the part of an ePub3 CFI which leads to the
     *content* of the referenced item.  If the identified manifest item is not referenced by a spine item,
     an empty string will be returned.
     @remarks The CFI spec states that all items be accessed via the spine.  Is it
     allowed to have a CFI which directly references a manifest item in the case
     where it's not referenced by a spine item?
     */
    string                  CFISubpathForManifestItemWithID(const string& ident) const;
    
    /**
     Get an array of manifest items which have *all* the given properties.
     @param A vector of manifest `<item>` property names, e.g. `"nav"`, `"cover"`, etc.
     @result A vector containing pointers to any matching manifest items.
     */
    const std::vector<const ManifestItem*> ManifestItemsWithProperties(PropertyList properties) const;
    
    /** @} */
    
    /**
     Returns a navigation table identified by type.
     @param type An `epub:type` attribute value, such as `"toc"` or `"lot"`.
     @result A pointer to the relevant navigation table, or `nullptr` if no such
     table was found.
     */
    const NavigationTable * NavigationTable(const string& type)            const;
    
    /**
     @defgroup PropertyIRIs Metadata Property IRI Support
     @{
     */
    
    /**
     Associates a property vocabulary IRI stem with a prefix.
     @param prefix The prefix used to identify the vocabulary IRI stem.
     @param iriStem The stem of the IRI for this vocabulary.
     */
    void                    RegisterPrefixIRIStem(const string& prefix, const string& iriStem);
    
    /**
     Creates a canonical IRI used to identify a metadata property (an item's type).
     @param reference The reference part of a `property` attribute's value, i.e. the
     part of the value following a `:` character, or the entire string if no `:` is
     present.
     @param prefix The prefix for the property, if supplied. If no prefix was
     supplied in the property, the default value for this parameter will identify
     the default vocabulary.
     @result The canonical IRI used to identify the property.
     @throws UnknownPrefix if the prefix has not been registered.
     */
    IRI                     MakePropertyIRI(const string& reference, const string& prefix="")   const;
    
    /**
     Creates a property IRI directly from a metadata element's `property` attribute
     value.
     @seealso MakePropertyIRI(const string&, const string&)
     @param attrValue The verbatim content of a metadata item's `property` attribute.
     @result The canonical IRI used to identify the property.
     @throws UnknownPrefix if the value contains a prefix which has not been
     registered.
     @throws std::invalid_argument If the value doesn't look to be in the correct
     format.
     */
    IRI                     PropertyIRIFromAttributeValue(const string& attrValue)              const;
    
    /// Returns the CFI node index for the `<spine>` element within the package
    /// document.
    uint32_t                SpineCFIIndex()                 const   { return _spineCFIIndex; }
    
protected:
    Archive *               _archive;           ///< The archive from which the package was loaded.
    xmlDocPtr               _opf;               ///< The XML document representing the package.
    string                  _pathBase;          ///< The base path of the document within the archive.
    string                  _type;              ///< The MIME type of the package document.
    MetadataMap             _metadata;          ///< All metadata from the package, in document order.
    ManifestTable           _manifest;          ///< All manifest items, indexed by unique identifier.
    NavigationMap           _navigation;        ///< All navigation tables, indexed by type.
    Auto<SpineItem>         _spine;             ///< The first item in the spine (SpineItems are a linked list).
    
    PropertyVocabularyMap   _vocabularyLookup;  ///< A lookup table for property prefix->IRI-stem mappings.
    
    // used to initialize each package's vocabulary map
    static const PropertyVocabularyMap gReservedVocabularies;           ///< The reserved/predefined metadata property vocabularies.
    static const std::map<Metadata::DCType, const IRI> gDCTypeIRIs;     ///< Our custom IRI mappings for DCMES metadata elements.
    
    // used to verify/correct CFIs
    uint32_t                _spineCFIIndex;     ///< The CFI index for the `<spine>` element in the package document.
    
    ///
    /// Unpacks the _opf document. Implemented by the subclass, to make PackageBase pure-virtual.
    virtual bool            Unpack() = 0;
    ///
    /// Used to handle the `prefix` attribute of the OPF `<package>` element.
    void                    InstallPrefixesFromAttributeValue(const string& attrValue);
    
    /**
     Locates a spine item based on the corresponding CFI component.
     
     May update the supplied CFI component following target correction.
     */
    const SpineItem *       ConfirmOrCorrectSpineItemQualifier(const SpineItem * pItem, CFI::Component* pComponent) const;
    
    ///
    /// Loads navigation tables from a given manifest item (which has the `"nav"` property).
    static NavigationList   NavTablesFromManifestItem(const ManifestItem * pItem);
};

/**
 The Package class implements a high-level API for interacting with OPF packages,
 including convenience accessors for well-known metadata items.
 */
class Package : public PackageBase
{
public:
                            Package()                                   = delete;
                            Package(Archive * archive, const string& path, const string& type);
                            Package(const Package&)                     = delete;
                            Package(Package&& o) : PackageBase(std::move(o)) {}
    virtual                 ~Package() {}
    
    // the full Unique Identifier, built from the package unique-id and the modification date.
    virtual string          UniqueID()              const;
    // the package unique-id on its own
    virtual string          PackageID()             const;
    // MIME type
    virtual const string&   Type()                  const       { return _type; }
    // opf version
    virtual string          Version()               const;
    
    const MetadataMap       MetadataItemsWithDCType(Metadata::DCType type) const;
    const MetadataMap       MetadataItemsWithProperty(const IRI& iri) const;
    
    const SpineItem *       SpineItemWithIDRef(const string& idref)         const;
    
    const CFI               CFIForManifestItem(const ManifestItem* item)    const;
    const CFI               CFIForSpineItem(const SpineItem* item)          const;
    
    // note that the CFI is purposely non-const so the package can correct it (cf. epub-cfi §3.5)
    const ManifestItem *    ManifestItemForCFI(CFI& cfi, CFI* pRemainingCFI) const;
    xmlDocPtr               DocumentForCFI(CFI& cfi, CFI* pRemainingCFI) const {
        return ManifestItemForCFI(cfi, pRemainingCFI)->ReferencedDocument();
    }
    
    // array-style operators: indices get spine items, identifiers get manifest items
    const SpineItem *       operator[](size_t idx)          const   { return SpineItemAt(idx); }
    const ManifestItem *    operator[](const string& ident) const   { return ManifestItemWithID(ident); }
    
    ArchiveReader*          ReaderForRelativePath(const string& path) const {
        return _archive->ReaderAtPath((_pathBase + path).stl_str());
    }
    ArchiveXmlReader*       XmlReaderForRelativePath(const string& path) const {
        return new ArchiveXmlReader(ReaderForRelativePath(path));
    }
    
    const class NavigationTable*    TableOfContents()           const       { return NavigationTable("toc"); }
    const class NavigationTable*    ListOfFigures()             const       { return NavigationTable("lof"); }
    const class NavigationTable*    ListOfIllustrations()       const       { return NavigationTable("loi"); }
    const class NavigationTable*    ListOfTables()              const       { return NavigationTable("lot"); }
    const class NavigationTable*    PageList()                  const       { return NavigationTable("pagelist"); }
    
    const string            Title()                     const;
    const string            Subtitle()                  const;
    const string            FullTitle()                 const;
    
    typedef std::vector<const string>     AttributionList;
    
    // returns the author names
    const AttributionList   AuthorNames()               const;
    // returns the file-as names if available, as Authors() if not
    const AttributionList   AttributionNames()          const;
    // returns a formatted string for presentation to the user
    const string            Authors()                   const;
    
    const string            Language()                  const;
    const string            Source()                    const;
    const string            CopyrightOwner()            const;
    const string            ModificationDate()          const;
    const string            ISBN()                      const;
    
    typedef std::vector<const string>     StringList;
    const StringList        Subjects()                  const;
    
protected:
    virtual bool            Unpack();
};

EPUB3_END_NAMESPACE

#endif /* defined(__ePub3__package__) */
