//
//  ViewController.m
//  sample
//
//  Created by good afternoon on 12/11/24.
//

#import "ViewController.h"
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface ViewController ()

@property (weak, nonatomic) IBOutlet UITextView *textFileOutput;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}

// button action to choose a file
- (IBAction)chooseFileTapped:(id)sender {
    // ask for a file picker by type txt
    NSArray *documentTypes = @[UTTypeText, UTTypePlainText];
    
    //UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:@[@"public.text"] inMode:UIDocumentPickerModeOpen];
    UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:documentTypes];
    documentPicker.delegate = self;
    [self presentViewController:documentPicker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentAtURL:(nonnull NSURL *)url {
    NSURL *fileURL = url;
    NSError *error;
    NSString *fileContents = [NSString stringWithContentsOfURL:fileURL encoding:NSUTF8StringEncoding error:&error];
    if (error) {
        self.textFileOutput.text = @"Error reading file!";
    } else {
        self.textFileOutput.text = fileContents;
    }
}


@end
