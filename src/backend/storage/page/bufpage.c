/*-------------------------------------------------------------------------
 *
 * bufpage.c
 *	  POSTGRES standard buffer page code.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/page/bufpage.c,v 1.71 2007/02/21 20:02:17 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "storage/bufpage.h"


/* ----------------------------------------------------------------
 *						Page support functions
 * ----------------------------------------------------------------
 */

/*
 * PageInit
 *		Initializes the contents of a page.
 */
void
PageInit(Page page, Size pageSize, Size specialSize)
{
	PageHeader	p = (PageHeader) page;

	specialSize = MAXALIGN(specialSize);

	Assert(pageSize == BLCKSZ);
	Assert(pageSize > specialSize + SizeOfPageHeaderData);

	/* Make sure all fields of page are zero, as well as unused space */
	MemSet(p, 0, pageSize);

	/* p->pd_flags = 0;								done by above MemSet */
	p->pd_lower = SizeOfPageHeaderData;
	p->pd_upper = pageSize - specialSize;
	p->pd_special = pageSize - specialSize;
	PageSetPageSizeAndVersion(page, pageSize, PG_PAGE_LAYOUT_VERSION);
}


/*
 * PageHeaderIsValid
 *		Check that the header fields of a page appear valid.
 *
 * This is called when a page has just been read in from disk.	The idea is
 * to cheaply detect trashed pages before we go nuts following bogus item
 * pointers, testing invalid transaction identifiers, etc.
 *
 * It turns out to be necessary to allow zeroed pages here too.  Even though
 * this routine is *not* called when deliberately adding a page to a relation,
 * there are scenarios in which a zeroed page might be found in a table.
 * (Example: a backend extends a relation, then crashes before it can write
 * any WAL entry about the new page.  The kernel will already have the
 * zeroed page in the file, and it will stay that way after restart.)  So we
 * allow zeroed pages here, and are careful that the page access macros
 * treat such a page as empty and without free space.  Eventually, VACUUM
 * will clean up such a page and make it usable.
 */
bool
PageHeaderIsValid(PageHeader page)
{
	char	   *pagebytes;
	int			i;

	/* Check normal case */
	if (PageGetPageSize(page) == BLCKSZ &&
		PageGetPageLayoutVersion(page) == PG_PAGE_LAYOUT_VERSION &&
		(page->pd_flags & ~PD_VALID_FLAG_BITS) == 0 &&
		page->pd_lower >= SizeOfPageHeaderData &&
		page->pd_lower <= page->pd_upper &&
		page->pd_upper <= page->pd_special &&
		page->pd_special <= BLCKSZ &&
		page->pd_special == MAXALIGN(page->pd_special))
		return true;

	/* Check all-zeroes case */
	pagebytes = (char *) page;
	for (i = 0; i < BLCKSZ; i++)
	{
		if (pagebytes[i] != 0)
			return false;
	}
	return true;
}


/*
 *	PageAddItem
 *
 *	Add an item to a page.	Return value is offset at which it was
 *	inserted, or InvalidOffsetNumber if there's not room to insert.
 *
 *	If offsetNumber is valid and <= current max offset in the page,
 *	insert item into the array at that position by shuffling ItemId's
 *	down to make room.
 *	If offsetNumber is not valid, then assign one by finding the first
 *	one that is both unused and deallocated.
 *
 *	!!! EREPORT(ERROR) IS DISALLOWED HERE !!!
 */
OffsetNumber
PageAddItem(Page page,
			Item item,
			Size size,
			OffsetNumber offsetNumber,
			ItemIdFlags flags)
{
	PageHeader	phdr = (PageHeader) page;
	Size		alignedSize;
	int			lower;
	int			upper;
	ItemId		itemId;
	OffsetNumber limit;
	bool		needshuffle = false;
	bool		overwritemode = (flags & OverwritePageMode) != 0;

	flags &= ~OverwritePageMode;

	/*
	 * Be wary about corrupted page pointers
	 */
	if (phdr->pd_lower < SizeOfPageHeaderData ||
		phdr->pd_lower > phdr->pd_upper ||
		phdr->pd_upper > phdr->pd_special ||
		phdr->pd_special > BLCKSZ)
		ereport(PANIC,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u",
						phdr->pd_lower, phdr->pd_upper, phdr->pd_special)));

	/*
	 * Select offsetNumber to place the new item at
	 */
	limit = OffsetNumberNext(PageGetMaxOffsetNumber(page));

	/* was offsetNumber passed in? */
	if (OffsetNumberIsValid(offsetNumber))
	{
		/* yes, check it */
		if (overwritemode)
		{
			if (offsetNumber < limit)
			{
				itemId = PageGetItemId(phdr, offsetNumber);
				if (ItemIdIsUsed(itemId) || ItemIdGetLength(itemId) != 0)
				{
					elog(WARNING, "will not overwrite a used ItemId");
					return InvalidOffsetNumber;
				}
			}
		}
		else
		{
			if (offsetNumber < limit)
				needshuffle = true;		/* need to move existing linp's */
		}
	}
	else
	{
		/* offsetNumber was not passed in, so find a free slot */
		/* if no free slot, we'll put it at limit (1st open slot) */
		if (PageHasFreeLinePointers(phdr))
		{
			/* look for "recyclable" (unused & deallocated) ItemId */
			for (offsetNumber = 1; offsetNumber < limit; offsetNumber++)
			{
				itemId = PageGetItemId(phdr, offsetNumber);
				if (!ItemIdIsUsed(itemId) && ItemIdGetLength(itemId) == 0)
					break;
			}
			if (offsetNumber >= limit)
			{
				/* the hint is wrong, so reset it */
				PageClearHasFreeLinePointers(phdr);
			}
		}
		else
		{
			/* don't bother searching if hint says there's no free slot */
			offsetNumber = limit;
		}
	}

	if (offsetNumber > limit)
	{
		elog(WARNING, "specified item offset is too large");
		return InvalidOffsetNumber;
	}

	/*
	 * Compute new lower and upper pointers for page, see if it'll fit.
	 *
	 * Note: do arithmetic as signed ints, to avoid mistakes if, say,
	 * alignedSize > pd_upper.
	 */
	if (offsetNumber == limit || needshuffle)
		lower = phdr->pd_lower + sizeof(ItemIdData);
	else
		lower = phdr->pd_lower;

	alignedSize = MAXALIGN(size);

	upper = (int) phdr->pd_upper - (int) alignedSize;

	if (lower > upper)
		return InvalidOffsetNumber;

	/*
	 * OK to insert the item.  First, shuffle the existing pointers if needed.
	 */
	itemId = PageGetItemId(phdr, offsetNumber);

	if (needshuffle)
		memmove(itemId + 1, itemId,
				(limit - offsetNumber) * sizeof(ItemIdData));

	/* set the item pointer */
	itemId->lp_off = upper;
	itemId->lp_len = size;
	itemId->lp_flags = flags;

	/* copy the item's data onto the page */
	memcpy((char *) page + upper, item, size);

	/* adjust page header */
	phdr->pd_lower = (LocationIndex) lower;
	phdr->pd_upper = (LocationIndex) upper;

	return offsetNumber;
}

/*
 * PageGetTempPage
 *		Get a temporary page in local memory for special processing
 */
Page
PageGetTempPage(Page page, Size specialSize)
{
	Size		pageSize;
	Size		size;
	Page		temp;
	PageHeader	thdr;

	pageSize = PageGetPageSize(page);
	temp = (Page) palloc(pageSize);
	thdr = (PageHeader) temp;

	/* copy old page in */
	memcpy(temp, page, pageSize);

	/* clear out the middle */
	size = pageSize - SizeOfPageHeaderData;
	size -= MAXALIGN(specialSize);
	MemSet(PageGetContents(thdr), 0, size);

	/* set high, low water marks */
	thdr->pd_lower = SizeOfPageHeaderData;
	thdr->pd_upper = pageSize - MAXALIGN(specialSize);

	return temp;
}

/*
 * PageRestoreTempPage
 *		Copy temporary page back to permanent page after special processing
 *		and release the temporary page.
 */
void
PageRestoreTempPage(Page tempPage, Page oldPage)
{
	Size		pageSize;

	pageSize = PageGetPageSize(tempPage);
	memcpy((char *) oldPage, (char *) tempPage, pageSize);

	pfree(tempPage);
}

/*
 * sorting support for PageRepairFragmentation, PageIndexMultiDelete,
 * PageIndexDeleteNoCompact
 */
typedef struct itemIdSortData
{
	int			offsetindex;	/* linp array index */
	int			itemoff;		/* page offset of item data */
	Size		alignedlen;		/* MAXALIGN(item data len) */
	ItemIdData	olditemid;		/* used only in PageIndexMultiDelete */
} itemIdSortData;
typedef itemIdSortData *itemIdSort;

static int
itemoffcompare(const void *itemidp1, const void *itemidp2)
{
	/* Sort in decreasing itemoff order */
	return ((itemIdSort) itemidp2)->itemoff -
		((itemIdSort) itemidp1)->itemoff;
}

/*
 * PageRepairFragmentation
 *
 * Frees fragmented space on a page.
 * It doesn't remove unused line pointers! Please don't change this.
 *
 * This routine is usable for heap pages only, but see PageIndexMultiDelete.
 *
 * Returns number of unused line pointers on page.	If "unused" is not NULL
 * then the unused[] array is filled with indexes of unused line pointers.
 */
int
PageRepairFragmentation(Page page, OffsetNumber *unused)
{
	Offset		pd_lower = ((PageHeader) page)->pd_lower;
	Offset		pd_upper = ((PageHeader) page)->pd_upper;
	Offset		pd_special = ((PageHeader) page)->pd_special;
	itemIdSort	itemidbase,
				itemidptr;
	ItemId		lp;
	int			nline,
				nused;
	int			i;
	Size		totallen;
	Offset		upper;

	/*
	 * It's worth the trouble to be more paranoid here than in most places,
	 * because we are about to reshuffle data in (what is usually) a shared
	 * disk buffer.  If we aren't careful then corrupted pointers, lengths,
	 * etc could cause us to clobber adjacent disk buffers, spreading the data
	 * loss further.  So, check everything.
	 */
	if (pd_lower < SizeOfPageHeaderData ||
		pd_lower > pd_upper ||
		pd_upper > pd_special ||
		pd_special > BLCKSZ ||
		pd_special != MAXALIGN(pd_special))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u",
						pd_lower, pd_upper, pd_special),
				 errSendAlert(true)));

	nline = PageGetMaxOffsetNumber(page);
	nused = 0;
	for (i = 0; i < nline; i++)
	{
		lp = PageGetItemId(page, i + 1);
		if (ItemIdDeleted(lp))	/* marked for deletion */
			lp->lp_flags &= ~(LP_USED | LP_DELETE);
		if (ItemIdIsUsed(lp))
			nused++;
		else if (unused)
			unused[i - nused] = (OffsetNumber) i;
	}

	if (nused == 0)
	{
		/* Page is completely empty, so just reset it quickly */
		for (i = 0; i < nline; i++)
		{
			lp = PageGetItemId(page, i + 1);
			lp->lp_len = 0;		/* indicate unused & deallocated */
		}
		((PageHeader) page)->pd_upper = pd_special;
	}
	else
	{							/* nused != 0 */
		/* Need to compact the page the hard way */
		itemidbase = (itemIdSort) palloc(sizeof(itemIdSortData) * nused);
		itemidptr = itemidbase;
		totallen = 0;
		for (i = 0; i < nline; i++)
		{
			lp = PageGetItemId(page, i + 1);
			if (ItemIdIsUsed(lp))
			{
				itemidptr->offsetindex = i;
				itemidptr->itemoff = ItemIdGetOffset(lp);
				if (itemidptr->itemoff < (int) pd_upper ||
					itemidptr->itemoff >= (int) pd_special)
					ereport(ERROR,
							(errcode(ERRCODE_DATA_CORRUPTED),
							 errmsg("corrupted item pointer: %u",
									itemidptr->itemoff),
							 errSendAlert(true)));
				itemidptr->alignedlen = MAXALIGN(ItemIdGetLength(lp));
				totallen += itemidptr->alignedlen;
				itemidptr++;
			}
			else
			{
				lp->lp_len = 0; /* indicate unused & deallocated */
			}
		}

		if (totallen > (Size) (pd_special - pd_lower))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
			   errmsg("corrupted item lengths: total %u, available space %u",
					  (unsigned int) totallen, pd_special - pd_lower),
			   errSendAlert(true)));

		/* sort itemIdSortData array into decreasing itemoff order */
		qsort((char *) itemidbase, nused, sizeof(itemIdSortData),
			  itemoffcompare);

		/* compactify page */
		upper = pd_special;

		for (i = 0, itemidptr = itemidbase; i < nused; i++, itemidptr++)
		{
			lp = PageGetItemId(page, itemidptr->offsetindex + 1);
			upper -= itemidptr->alignedlen;
			memmove((char *) page + upper,
					(char *) page + itemidptr->itemoff,
					itemidptr->alignedlen);
			lp->lp_off = upper;
		}

		((PageHeader) page)->pd_upper = upper;

		pfree(itemidbase);
	}

	/* Set hint bit for PageAddItem */
	if (nused < nline)
		PageSetHasFreeLinePointers(page);
	else
		PageClearHasFreeLinePointers(page);

	return (nline - nused);
}

/*
 * PageGetFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		deducted by the space needed for a new line pointer.
 */
Size
PageGetFreeSpace(Page page)
{
	int			space;

	/*
	 * Use signed arithmetic here so that we behave sensibly if pd_lower >
	 * pd_upper.
	 */
	space = (int) ((PageHeader) page)->pd_upper -
		(int) ((PageHeader) page)->pd_lower;

	if (space < (int) sizeof(ItemIdData))
		return 0;
	space -= sizeof(ItemIdData);

	return (Size) space;
}

/*
 * PageGetExactFreeSpace
 *		Returns the size of the free (allocatable) space on a page.
 */
Size
PageGetExactFreeSpace(Page page)
{
	int			space;

	/*
	 * Use signed arithmetic here so that we behave sensibly if pd_lower >
	 * pd_upper.
	 */
	space = (int) ((PageHeader) page)->pd_upper -
		(int) ((PageHeader) page)->pd_lower;

	return (Size) space;
}


/*
 * PageIndexTupleDelete
 *
 * This routine does the work of removing a tuple from an index page.
 *
 * Unlike heap pages, we compact out the line pointer for the removed tuple.
 */
void
PageIndexTupleDelete(Page page, OffsetNumber offnum)
{
	PageHeader	phdr = (PageHeader) page;
	char	   *addr;
	ItemId		tup;
	Size		size;
	unsigned	offset;
	int			nbytes;
	int			offidx;
	int			nline;

	/*
	 * As with PageRepairFragmentation, paranoia seems justified.
	 */
	if (phdr->pd_lower < SizeOfPageHeaderData ||
		phdr->pd_lower > phdr->pd_upper ||
		phdr->pd_upper > phdr->pd_special ||
		phdr->pd_special > BLCKSZ)
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u",
						phdr->pd_lower, phdr->pd_upper, phdr->pd_special),
				 errSendAlert(true)));

	nline = PageGetMaxOffsetNumber(page);
	if ((int) offnum <= 0 || (int) offnum > nline)
		elog(ERROR, "invalid index offnum: %u", offnum);

	/* change offset number to offset index */
	offidx = offnum - 1;

	tup = PageGetItemId(page, offnum);
	size = ItemIdGetLength(tup);
	offset = ItemIdGetOffset(tup);

	if (offset < phdr->pd_upper || (offset + size) > phdr->pd_special ||
		offset != MAXALIGN(offset) || size != MAXALIGN(size))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("corrupted item pointer: offset = %u, size = %u",
						offset, (unsigned int) size),
				 errSendAlert(true)));

	/*
	 * First, we want to get rid of the pd_linp entry for the index tuple. We
	 * copy all subsequent linp's back one slot in the array. We don't use
	 * PageGetItemId, because we are manipulating the _array_, not individual
	 * linp's.
	 */
	nbytes = phdr->pd_lower -
		((char *) &phdr->pd_linp[offidx + 1] - (char *) phdr);

	if (nbytes > 0)
		memmove((char *) &(phdr->pd_linp[offidx]),
				(char *) &(phdr->pd_linp[offidx + 1]),
				nbytes);

	/*
	 * Now move everything between the old upper bound (beginning of tuple
	 * space) and the beginning of the deleted tuple forward, so that space in
	 * the middle of the page is left free.  If we've just deleted the tuple
	 * at the beginning of tuple space, then there's no need to do the copy
	 * (and bcopy on some architectures SEGV's if asked to move zero bytes).
	 */

	/* beginning of tuple space */
	addr = (char *) page + phdr->pd_upper;

	if (offset > phdr->pd_upper)
		memmove(addr + size, addr, (int) (offset - phdr->pd_upper));

	/* adjust free space boundary pointers */
	phdr->pd_upper += size;
	phdr->pd_lower -= sizeof(ItemIdData);

	/*
	 * Finally, we need to adjust the linp entries that remain.
	 *
	 * Anything that used to be before the deleted tuple's data was moved
	 * forward by the size of the deleted tuple.
	 */
	if (!PageIsEmpty(page))
	{
		int			i;

		nline--;				/* there's one less than when we started */
		for (i = 1; i <= nline; i++)
		{
			ItemId		ii = PageGetItemId(phdr, i);

			if (ItemIdGetOffset(ii) <= offset)
				ii->lp_off += size;
		}
	}
}


/*
 * PageIndexMultiDelete
 *
 * This routine handles the case of deleting multiple tuples from an
 * index page at once.	It is considerably faster than a loop around
 * PageIndexTupleDelete ... however, the caller *must* supply the array
 * of item numbers to be deleted in item number order!
 */
void
PageIndexMultiDelete(Page page, OffsetNumber *itemnos, int nitems)
{
	PageHeader	phdr = (PageHeader) page;
	Offset		pd_lower = phdr->pd_lower;
	Offset		pd_upper = phdr->pd_upper;
	Offset		pd_special = phdr->pd_special;
	itemIdSort	itemidbase,
				itemidptr;
	ItemId		lp;
	int			nline,
				nused;
	int			i;
	Size		totallen;
	Offset		upper;
	Size		size;
	unsigned	offset;
	int			nextitm;
	OffsetNumber offnum;

	/*
	 * If there aren't very many items to delete, then retail
	 * PageIndexTupleDelete is the best way.  Delete the items in reverse
	 * order so we don't have to think about adjusting item numbers for
	 * previous deletions.
	 *
	 * TODO: tune the magic number here
	 */
	if (nitems <= 2)
	{
		while (--nitems >= 0)
			PageIndexTupleDelete(page, itemnos[nitems]);
		return;
	}

	/*
	 * As with PageRepairFragmentation, paranoia seems justified.
	 */
	if (pd_lower < SizeOfPageHeaderData ||
		pd_lower > pd_upper ||
		pd_upper > pd_special ||
		pd_special > BLCKSZ ||
		pd_special != MAXALIGN(pd_special))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u",
						pd_lower, pd_upper, pd_special),
				 errSendAlert(true)));

	/*
	 * Scan the item pointer array and build a list of just the ones we are
	 * going to keep.  Notice we do not modify the page yet, since we are
	 * still validity-checking.
	 */
	nline = PageGetMaxOffsetNumber(page);
	itemidbase = (itemIdSort) palloc(sizeof(itemIdSortData) * nline);
	itemidptr = itemidbase;
	totallen = 0;
	nused = 0;
	nextitm = 0;
	for (offnum = 1; offnum <= nline; offnum++)
	{
		lp = PageGetItemId(page, offnum);
		size = ItemIdGetLength(lp);
		offset = ItemIdGetOffset(lp);
		if (offset < pd_upper ||
			(offset + size) > pd_special ||
			offset != MAXALIGN(offset))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("corrupted item pointer: offset = %u, size = %u",
							offset, (unsigned int) size),
					 errSendAlert(true)));

		if (nextitm < nitems && offnum == itemnos[nextitm])
		{
			/* skip item to be deleted */
			nextitm++;
		}
		else
		{
			itemidptr->offsetindex = nused;		/* where it will go */
			itemidptr->itemoff = offset;
			itemidptr->olditemid = *lp;
			itemidptr->alignedlen = MAXALIGN(size);
			totallen += itemidptr->alignedlen;
			itemidptr++;
			nused++;
		}
	}

	/* this will catch invalid or out-of-order itemnos[] */
	if (nextitm != nitems)
		elog(ERROR, "incorrect index offsets supplied");

	if (totallen > (Size) (pd_special - pd_lower))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
			   errmsg("corrupted item lengths: total %u, available space %u",
					  (unsigned int) totallen, pd_special - pd_lower),
			   errSendAlert(true)));

	/* sort itemIdSortData array into decreasing itemoff order */
	qsort((char *) itemidbase, nused, sizeof(itemIdSortData),
		  itemoffcompare);

	/* compactify page and install new itemids */
	upper = pd_special;

	for (i = 0, itemidptr = itemidbase; i < nused; i++, itemidptr++)
	{
		lp = PageGetItemId(page, itemidptr->offsetindex + 1);
		upper -= itemidptr->alignedlen;
		memmove((char *) page + upper,
				(char *) page + itemidptr->itemoff,
				itemidptr->alignedlen);
		*lp = itemidptr->olditemid;
		lp->lp_off = upper;
	}

	phdr->pd_lower = SizeOfPageHeaderData + nused * sizeof(ItemIdData);
	phdr->pd_upper = upper;

	pfree(itemidbase);
}

/*
 * PageIndexDeleteNoCompact
 *		Delete the given items for an index page, and defragment the resulting
 *		free space, but do not compact the item pointers array.
 *
 * itemnos is the array of tuples to delete; nitems is its size.  maxIdxTuples
 * is the maximum number of tuples that can exist in a page.
 *
 * Unused items at the end of the array are removed.
 *
 * This is used for index AMs that require that existing TIDs of live tuples
 * remain unchanged.
 */
void
PageIndexDeleteNoCompact(Page page, OffsetNumber *itemnos, int nitems)
{
	PageHeader	phdr = (PageHeader) page;
	LocationIndex pd_lower = phdr->pd_lower;
	LocationIndex pd_upper = phdr->pd_upper;
	LocationIndex pd_special = phdr->pd_special;
	int			nline;
	bool		empty;
	OffsetNumber offnum;
	int			nextitm;

	/*
	 * As with PageRepairFragmentation, paranoia seems justified.
	 */
	if (pd_lower < SizeOfPageHeaderData ||
		pd_lower > pd_upper ||
		pd_upper > pd_special ||
		pd_special > BLCKSZ ||
		pd_special != MAXALIGN(pd_special))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u",
						pd_lower, pd_upper, pd_special)));

	/*
	 * Scan the existing item pointer array and mark as unused those that are
	 * in our kill-list; make sure any non-interesting ones are marked unused
	 * as well.
	 */
	nline = PageGetMaxOffsetNumber(page);
	empty = true;
	nextitm = 0;
	for (offnum = FirstOffsetNumber; offnum <= nline; offnum = OffsetNumberNext(offnum))
	{
		ItemId		lp;
		ItemLength	itemlen;
		ItemOffset	offset;

		lp = PageGetItemId(page, offnum);

		itemlen = ItemIdGetLength(lp);
		offset = ItemIdGetOffset(lp);

		if (ItemIdIsUsed(lp))
		{
			if (offset < pd_upper ||
				(offset + itemlen) > pd_special ||
				offset != MAXALIGN(offset))
				ereport(ERROR,
						(errcode(ERRCODE_DATA_CORRUPTED),
						 errmsg("corrupted item pointer: offset = %u, length = %u",
								offset, (unsigned int) itemlen)));

			if (nextitm < nitems && offnum == itemnos[nextitm])
			{
				/* this one is on our list to delete, so mark it unused */
				ItemIdSetUnused(lp);
				nextitm++;
			}
			else if (ItemIdHasStorage(lp))
			{
				/* This one's live -- must do the compaction dance */
				empty = false;
			}
			else
			{
				/* get rid of this one too */
				ItemIdSetUnused(lp);
			}
		}
	}

	/* this will catch invalid or out-of-order itemnos[] */
	if (nextitm != nitems)
		elog(ERROR, "incorrect index offsets supplied");

	if (empty)
	{
		/* Page is completely empty, so just reset it quickly */
		phdr->pd_lower = SizeOfPageHeaderData;
		phdr->pd_upper = pd_special;
	}
	else
	{
		/* There are live items: need to compact the page the hard way */
		itemIdSortData itemidbase[MaxOffsetNumber];
		itemIdSort	itemidptr;
		int			i;
		Size		totallen;
		Offset		upper;

		/*
		 * Scan the page taking note of each item that we need to preserve.
		 * This includes both live items (those that contain data) and
		 * interspersed unused ones.  It's critical to preserve these unused
		 * items, because otherwise the offset numbers for later live items
		 * would change, which is not acceptable.  Unused items might get used
		 * again later; that is fine.
		 */
		itemidptr = itemidbase;
		totallen = 0;
		for (i = 0; i < nline; i++, itemidptr++)
		{
			ItemId		lp;

			itemidptr->offsetindex = i;

			lp = PageGetItemId(page, i + 1);
			if (ItemIdHasStorage(lp))
			{
				itemidptr->itemoff = ItemIdGetOffset(lp);
				itemidptr->alignedlen = MAXALIGN(ItemIdGetLength(lp));
				totallen += itemidptr->alignedlen;
			}
			else
			{
				itemidptr->itemoff = 0;
				itemidptr->alignedlen = 0;
			}
		}
		/* By here, there are exactly nline elements in itemidbase array */

		if (totallen > (Size) (pd_special - pd_lower))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("corrupted item lengths: total %u, available space %u",
							(unsigned int) totallen, pd_special - pd_lower)));

		/* sort itemIdSortData array into decreasing itemoff order */
		qsort((char *) itemidbase, nline, sizeof(itemIdSortData),
			  itemoffcompare);

		/*
		 * Defragment the data areas of each tuple, being careful to preserve
		 * each item's position in the linp array.
		 */
		upper = pd_special;
		PageClearHasFreeLinePointers(page);
		for (i = 0, itemidptr = itemidbase; i < nline; i++, itemidptr++)
		{
			ItemId		lp;

			lp = PageGetItemId(page, itemidptr->offsetindex + 1);
			if (itemidptr->alignedlen == 0)
			{
				PageSetHasFreeLinePointers(page);
				ItemIdSetUnused(lp);
				continue;
			}
			upper -= itemidptr->alignedlen;
			memmove((char *) page + upper,
					(char *) page + itemidptr->itemoff,
					itemidptr->alignedlen);
			lp->lp_off = upper;
			/* lp_flags and lp_len remain the same as originally */
		}

		/* Set the new page limits */
		phdr->pd_upper = upper;
		phdr->pd_lower = SizeOfPageHeaderData + i * sizeof(ItemIdData);
	}
}
