
module list
{
   enum TKind
   {
      small,
      medium,
      large
   }

   class TItem
   {
      attr <string> name;
      attr <TKind> kind { Init = medium; };
      attr <int> value;
   }

   class TItemList
   {
      list <TItem> items
      {
         Comment = "list of items";
         Suffix1 = Item;
         Suffix2 = Items;
         Up = owner;
      }

      attr <string> name;
   }
}
