#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Book ratings, in half stars (0 = not rated, 10 = five stars; see BookRatingText.h).
//
// A rating is written the moment it is given, in two places:
// - /.crosspoint/ratings.txt, one line a rated book (rating, title, author,
//   file, separated by tabs). Plain text, so it can be read on a computer and
//   survives firmware updates and a cleared book cache;
// - rating.txt beside the reader's other data for that book, which answers
//   when the list has lost the book's line.
namespace BookRatings {

constexpr char kListPath[] = "/.crosspoint/ratings.txt";

// 0 when the book is not rated.
uint8_t get(const std::string& bookPath);
// Saves the rating; 0 takes it away. title and author are written into the list
// for whoever reads it (an empty title leaves the file name to speak).
bool set(const std::string& bookPath, uint8_t halfStars, const std::string& title, const std::string& author);
// The book file was moved or renamed on the reader: its rating follows.
void bookMoved(const std::string& oldPath, const std::string& newPath);

// Every rating at once, for a list of books: the file is read one time and a
// book's rating is then found without touching the card.
class Index {
 public:
  void load();
  uint8_t find(const std::string& bookPath) const;
  bool empty() const { return entries_.empty(); }

 private:
  struct Entry {
    uint32_t pathHash;
    uint8_t halfStars;
  };
  std::vector<Entry> entries_;  // sorted by pathHash
};

}  // namespace BookRatings
