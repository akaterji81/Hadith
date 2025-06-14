import requests
import json

# API endpoint with your API key - using random=1 and limit=1 for a smaller response
api_url = 'https://hadithapi.com/api/hadiths?apiKey=$2y$10$FkcEZULcgznqRMIa5j46PesgqPvoWl9bMAlCJcfMqtKNDekDgMO&random=1&limit=1'

def test_api():
    print("Sending request to:", api_url)
    
    try:
        # Make the API request
        response = requests.get(api_url)
        
        # Check if request was successful
        if response.status_code == 200:
            print(f"Success! Status code: {response.status_code}")
            print(f"Response size: {len(response.text)} bytes")
            
            # Try to parse as JSON
            try:
                data = response.json()
                print("\nJSON structure:")
                print(list(data.keys()))
                
                # Check if hadiths key exists
                if 'hadiths' in data:
                    print("\nHadiths structure:")
                    print(list(data['hadiths'].keys()))
                    
                    # Count and display hadiths
                    if 'data' in data['hadiths']:
                        hadiths = data['hadiths']['data']
                        print(f"\nNumber of hadiths fetched: {len(hadiths)}")
                        
                        # Display each hadith
                        for i, hadith in enumerate(hadiths):
                            print(f"\n--- Hadith {i+1} ---")
                            print(f"ID: {hadith.get('id', 'N/A')}")
                            print(f"Book: {hadith.get('bookName', 'N/A')}")
                            print(f"Chapter: {hadith.get('chapterEnglish', 'N/A')}")
                            print(f"Hadith text: {hadith.get('hadithEnglish', 'N/A')}")
                            print(f"Narrator: {hadith.get('narratorEnglish', 'N/A')}")
                            
                            # Analyze hadith text for quotation marks
                            hadith_text = hadith.get('hadithEnglish', '')
                            if hadith_text.startswith('\"'):
                                print(f"NOTE: Hadith starts with quotation mark")
                                print(f"First character after quote: '{hadith_text[1:2]}'")
                            
                            # Check for Unicode characters
                            if '\\u' in hadith_text:
                                print(f"NOTE: Hadith contains Unicode escape sequences")
                            
                            print("\n")
                    
                    # Check if data array exists and has items
                    if 'data' in data['hadiths'] and len(data['hadiths']['data']) > 0:
                        first_hadith = data['hadiths']['data'][0]
                        print("\nFirst hadith keys:")
                        print(list(first_hadith.keys()))
                        
                        # Print the actual hadith text and source
                        print("\nSearching for hadith text field...")
                        hadith_field_found = False
                        
                        # Try different possible field names for the hadith text
                        for field in ['hadith', 'hadithEnglish', 'hadithText', 'text', 'content']:
                            if field in first_hadith:
                                print(f"Found hadith in field: {field}")
                                print("\nHadith text:")
                                print(first_hadith[field][:200] + "..." if len(first_hadith[field]) > 200 else first_hadith[field])
                                hadith_field_found = True
                                break
                        
                        if not hadith_field_found:
                            print("Could not find hadith text in any expected field")
                        
                        # Try to extract source information
                        print("\nSearching for source fields...")
                        book = None
                        chapter = None
                        
                        # Check for book field
                        if 'book' in first_hadith:
                            book = first_hadith['book']
                            print(f"Found book: {book}")
                        else:
                            print("Book field not found")
                            
                        # Check for chapter field
                        if 'chapter' in first_hadith:
                            chapter = first_hadith['chapter']
                            print(f"Found chapter: {chapter}")
                        else:
                            print("Chapter field not found")
                            
                        if book and chapter:
                            print("\nSource:")
                            print(f"{book} - {chapter}")
                    else:
                        print("No hadiths found in the response")
                else:
                    print("No 'hadiths' key in response")
                    print("Available keys:", list(data.keys()))
            
            except json.JSONDecodeError:
                print("Failed to parse JSON response")
                print("First 200 characters of response:")
                print(response.text[:200])
                
                # Try to manually find key fields in the response
                print("\nTrying manual string search...")
                hadith_pos = response.text.find('"hadith":"')
                if hadith_pos > 0:
                    print(f"Found 'hadith' at position {hadith_pos}")
        else:
            print(f"Request failed with status code: {response.status_code}")
    except Exception as e:
        print(f"An error occurred: {e}")


if __name__ == "__main__":
    test_api()
